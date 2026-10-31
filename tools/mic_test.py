#!/usr/bin/env python3
"""Receives recordings from the watch's microphone test and tries them on OpenAI:

    mic_test.py [port] [--out DIR] [--no-ai]

Build the firmware with CONFIG_KNOEDEL_MIC_TEST (menuconfig: Knödel Watch), hold
the right key and talk; on release the watch sends the recording over USB. Each
one is saved as a WAV file, transcribed (gpt-4o-mini-transcribe) and tidied into
a note (gpt-5-mini), as the watch will do it later. Without a port, the watch's
USB port is found by itself. Stop with Ctrl-C.

Needs pyserial; OPENAI_API_KEY in the environment for the transcription.
"""
import argparse
import base64
import datetime
import glob
import json
import os
import sys
import time
import urllib.error
import urllib.request
import uuid
import wave
from pathlib import Path

import serial

TRANSCRIBE_MODEL = "gpt-4o-mini-transcribe"
TIDY_MODEL = "gpt-5-mini"
# Describes the mix, so that the model writes each part as it was spoken. An example sentence
# here was given back as the transcript when the start of a memo was unclear: never use one.
TRANSCRIBE_PROMPT = "A voice memo. The speaker mixes Chinese, German and English, often within one sentence; write each word in the language it is spoken in."

# Tried on real memos: without the rule on reasons, "给房东打电话 wegen der Heizung"
# became two tasks, one of them made up. Reasoning effort "low" was slower and
# copied whole sentences; "minimal" is used.
TIDY_INSTRUCTIONS = (
    "You turn a short voice memo into a note for a small watch screen. The memo may mix Chinese, German "
    "and English, even within one sentence. Use the speaker's own words in the language they were spoken in; "
    "never translate or paraphrase, never add anything. Drop only filler words (嗯, 那个, 然后 at the start, "
    "äh, um) and repetitions. kind is 'todo' when the memo asks for things to do, else 'idea'. For a todo: "
    "one item per task; a reason or detail (wegen …, 因为 …, 关于 …) stays in the item of its task in the speaker's words, with no linking words added; "
    "it is not a task of its own. Keep items short. due is an ISO date only when the memo names a day for "
    "that task (a day said before several tasks joined by 然后/und/and counts for each of them); else null. "
    "text is ''. For an idea: items is empty and text is the tidied memo. Weekdays mean the next such day "
    "after now."
)
TIDY_SCHEMA = {
    "type": "object",
    "additionalProperties": False,
    "required": ["kind", "items", "text"],
    "properties": {
        "kind": {"type": "string", "enum": ["todo", "idea"]},
        "items": {
            "type": "array",
            "items": {
                "type": "object",
                "additionalProperties": False,
                "required": ["text", "due"],
                "properties": {"text": {"type": "string"}, "due": {"type": ["string", "null"]}},
            },
        },
        "text": {"type": "string"},
    },
}


def find_port():
    ports = glob.glob("/dev/cu.usbmodem*") + glob.glob("/dev/ttyACM*")
    if len(ports) != 1:
        sys.exit(f"give the port: {'no USB modem port found' if not ports else 'several found: ' + ', '.join(ports)}")
    return ports[0]


def openai(path, key, body, content_type):
    req = urllib.request.Request(f"https://api.openai.com/v1/{path}", data=body,
                                 headers={"Authorization": f"Bearer {key}", "Content-Type": content_type})
    with urllib.request.urlopen(req, timeout=90) as r:
        return json.load(r)


def transcribe(key, wav_path):
    boundary = uuid.uuid4().hex
    parts = []
    for name, value in (("model", TRANSCRIBE_MODEL), ("prompt", TRANSCRIBE_PROMPT)):
        parts.append(f'--{boundary}\r\nContent-Disposition: form-data; name="{name}"\r\n\r\n{value}\r\n'.encode())
    parts.append(f'--{boundary}\r\nContent-Disposition: form-data; name="file"; filename="memo.wav"\r\n'
                 f"Content-Type: audio/wav\r\n\r\n".encode() + wav_path.read_bytes() + b"\r\n")
    parts.append(f"--{boundary}--\r\n".encode())
    return openai("audio/transcriptions", key, b"".join(parts), f"multipart/form-data; boundary={boundary}")["text"]


def tidy(key, text):
    now = datetime.datetime.now().strftime("%A, %Y-%m-%d, %H:%M")
    body = {
        "model": TIDY_MODEL,
        "instructions": TIDY_INSTRUCTIONS,
        "input": f"Now: {now}\nMemo: {text}",
        "reasoning": {"effort": "minimal"},
        "text": {"format": {"type": "json_schema", "name": "note", "strict": True, "schema": TIDY_SCHEMA}},
    }
    out = openai("responses", key, json.dumps(body).encode(), "application/json")
    return json.loads(next(c["text"] for o in out["output"] if o["type"] == "message" for c in o["content"]))


def handle(samples_b64, rate, samples, out_dir, key):
    pcm = base64.b64decode("".join(samples_b64))
    if len(pcm) != samples * 2:
        print(f"  !! got {len(pcm) // 2} samples instead of {samples}: some lines were lost")
    name = datetime.datetime.now().strftime("%Y%m%d-%H%M%S")
    wav_path = out_dir / f"{name}.wav"
    with wave.open(str(wav_path), "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(rate)
        w.writeframes(pcm)
    print(f"  saved {wav_path}  ({len(pcm) / 2 / rate:.1f} s)")
    if not key:
        return
    result = {}
    try:
        t0 = time.time()
        result["transcript"] = transcribe(key, wav_path)
        t1 = time.time()
        print(f"  transcript ({t1 - t0:.1f} s): {result['transcript']}")
        result["note"] = tidy(key, result["transcript"])
        print(f"  note       ({time.time() - t1:.1f} s): {json.dumps(result['note'], ensure_ascii=False)}")
    except urllib.error.HTTPError as e:
        print(f"  !! OpenAI {e.code}: {e.read().decode(errors='replace')[:300]}")
    wav_path.with_suffix(".json").write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding="utf-8")


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("port", nargs="?")
    ap.add_argument("--out", type=Path, default=Path(__file__).resolve().parent.parent / "build" / "mic_test")
    ap.add_argument("--no-ai", action="store_true", help="only save the WAV files")
    args = ap.parse_args()
    key = None if args.no_ai else os.environ.get("OPENAI_API_KEY")
    if not args.no_ai and not key:
        print("OPENAI_API_KEY is not set: only saving the WAV files")
    args.out.mkdir(parents=True, exist_ok=True)

    # Opened without touching DTR/RTS, so that the watch is not reset.
    s = serial.Serial()
    s.port, s.baudrate, s.timeout = args.port or find_port(), 115200, 0.5
    s.dtr = s.rts = False
    s.open()
    print(f"listening on {s.port}: hold the right key on the watch and talk")
    lines, header = None, None
    while True:
        raw = s.readline()
        if not raw:
            continue
        line = raw.decode("utf-8", "replace").strip()
        if line.startswith("MIC_BEGIN"):
            _, rate, samples = line.split()
            header, lines = (int(rate), int(samples)), []
        elif line == "MIC_END" and lines is not None:
            handle(lines, *header, args.out, key)
            lines = None
        elif lines is not None:
            if line and len(line) <= 76 and all(c.isalnum() or c in "+/=" for c in line):
                lines.append(line)
            else:
                print(f"  (log in the middle: {line})")
        elif line:
            print(line)


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        pass
