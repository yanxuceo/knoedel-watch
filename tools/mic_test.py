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

# Tried on real memos (see main/openai.cpp): the same rules as the watch; days are counted here.
TIDY_INSTRUCTIONS = (
    "You turn a short voice memo into a note for a small watch screen. The memo may mix Chinese, German "
    "and English, even within one sentence. Copy the speaker's words exactly as spoken, each in its own "
    "language: never translate a single word (Milch stays Milch), never rephrase, never add a word. "
    "kind is 'todo' when the memo asks for things to do, else 'idea'. For a todo: one item per task, "
    "only the task itself: leave out filler and reminder words (嗯, 那个, 这个, 记得, 要, 然后, äh, um, I need to), "
    "the day words once they are in due (明天, 下周一, 之前, Freitag, tomorrow), and background that is "
    "not a task. A reason (wegen …, 因为 …) or a time of day (早晨七点, um 7 Uhr, at 9) stays in its task, it is "
    "not a task of its own. Inside a task, drop words said twice and fillers in the middle too "
    "(买一卷那个厕所的厕所的那个纸 becomes 买一卷厕所的纸); keep every other word. when: how the memo names "
    "that task's day, nothing more: type 'days' for 今天/明天/后天/heute/morgen/übermorgen/today/tomorrow (days "
    "0, 1, 2); 'weekday' for a day of the week (weekday 1 = Monday .. 7 = Sunday; next_week true only when the "
    "memo says 下周/下个星期/nächste Woche/next week, not for 这周/this/plain weekdays); 'date' for a calendar "
    "date (date as ISO, the year from today); 'none' when no day is named. A day said before several tasks "
    "joined by 然后/und/and counts for each of them. Never compute dates from weekdays yourself. text is ''. "
    "For an idea: items is empty "
    "and text is the memo without filler words."
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
                "required": ["text", "when"],
                "properties": {"text": {"type": "string"}, "when": {
                    "type": "object", "additionalProperties": False,
                    "required": ["type", "days", "weekday", "next_week", "date"],
                    "properties": {"type": {"type": "string", "enum": ["none", "days", "weekday", "date"]},
                                   "days": {"type": ["integer", "null"]}, "weekday": {"type": ["integer", "null"]},
                                   "next_week": {"type": "boolean"}, "date": {"type": ["string", "null"]}}}},
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
    today = datetime.date.today()
    context = f"Today: {today:%A, %Y-%m-%d}"
    body = {
        "model": TIDY_MODEL,
        "instructions": TIDY_INSTRUCTIONS,
        "input": f"{context}\nMemo: {text}",
        "reasoning": {"effort": "minimal"},
        "text": {"format": {"type": "json_schema", "name": "note", "strict": True, "schema": TIDY_SCHEMA}},
    }
    out = openai("responses", key, json.dumps(body).encode(), "application/json")
    note = json.loads(next(c["text"] for o in out["output"] if o["type"] == "message" for c in o["content"]))
    for item in note["items"]:  # counted here, as on the watch (main/notes_sync.cpp)
        day = count_day(item.pop("when"), today)
        item["due"] = day.isoformat() if day else None
    return note


def count_day(when, base):
    """"下周三" is the Wednesday of the week after, "周五" the next Friday, "明天" the day after."""
    if when["type"] == "days" and when["days"] is not None and 0 <= when["days"] < 400:
        return base + datetime.timedelta(when["days"])
    if when["type"] == "weekday" and when["weekday"] and 1 <= when["weekday"] <= 7:
        today = base.isoweekday()
        if when["next_week"]:
            return base + datetime.timedelta(7 - (today - 1) + (when["weekday"] - 1))
        return base + datetime.timedelta((when["weekday"] - today) % 7 or 7)
    if when["type"] == "date" and when["date"]:
        try:
            return datetime.date.fromisoformat(when["date"])
        except ValueError:
            return None
    return None


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
