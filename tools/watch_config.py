#!/usr/bin/env python3
"""Sets up the watch's notes over USB (main/usb_commands.h):

    watch_config.py wifi add "<network name>"   asks for the password (not shown)
    watch_config.py wifi list | wifi clear
    watch_config.py key set                     sends OPENAI_API_KEY from the environment
    watch_config.py key show                    the last four characters of the key on the watch
    watch_config.py net test                    goes online once and sets the watch's clock
    watch_config.py notes list                  the notes on the watch, newest first
    watch_config.py notes sync                  sends the memos waiting
    watch_config.py notes clear                 deletes the notes (not the memos waiting)

Options: --port PORT (else the only USB modem port). Nothing secret is printed.
Needs pyserial (the ESP-IDF Python has it).
"""
import argparse
import getpass
import glob
import os
import sys
import time

import serial


def find_port():
    ports = glob.glob("/dev/cu.usbmodem*") + glob.glob("/dev/ttyACM*")
    if len(ports) != 1:
        sys.exit(f"give --port: {'no USB modem port found' if not ports else 'several found: ' + ', '.join(ports)}")
    return ports[0]


def send(port, command, timeout):
    s = serial.Serial()
    s.port, s.baudrate, s.timeout = port, 115200, 0.2
    s.dtr = s.rts = False
    s.open()
    # Opening the port may restart the watch: wait until it is up and quiet.
    quiet_since, end = time.time(), time.time() + 8
    while time.time() < end and time.time() - quiet_since < 1.5:
        if s.read(4096):
            quiet_since = time.time()
    s.write((command + "\n").encode())
    s.flush()
    end = time.time() + timeout
    buf = b""
    while time.time() < end:
        buf += s.read(4096)
        lines = buf.decode("utf-8", "replace").splitlines()
        for i, line in enumerate(lines):
            if line.startswith(("OK", "ERR")):
                notes = [l[5:] for l in lines[:i] if l.startswith("NOTE ")]
                return "\n".join(notes + [line])
    return "ERR no answer from the watch (is it the firmware with notes?)"


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("what", choices=["wifi", "key", "net", "notes"])
    ap.add_argument("action")
    ap.add_argument("name", nargs="?")
    ap.add_argument("--port")
    args = ap.parse_args()

    timeout = 5
    if (args.what, args.action) == ("wifi", "add"):
        if not args.name:
            sys.exit('give the network name: watch_config.py wifi add "<name>"')
        password = os.environ.get("KNOEDEL_WIFI_PASSWORD") or getpass.getpass(f'password of "{args.name}" (empty: open network): ')
        if "\t" in args.name or "\n" in args.name + password:
            sys.exit("tabs and line breaks cannot be sent")
        command = f"wifi add {args.name}\t{password}"
    elif (args.what, args.action) in (("wifi", "list"), ("wifi", "clear"), ("key", "show")):
        command = f"{args.what} {args.action}"
    elif (args.what, args.action) == ("key", "set"):
        key = os.environ.get("OPENAI_API_KEY", "")
        if not key:
            sys.exit("OPENAI_API_KEY is not set")
        command = f"key set {key}"
    elif (args.what, args.action) == ("net", "test"):
        command, timeout = "net test", 60
    elif (args.what, args.action) in (("notes", "list"), ("notes", "clear")):
        command, timeout = f"notes {args.action}", 15
    elif (args.what, args.action) == ("notes", "sync"):
        command, timeout = "notes sync", 300
    else:
        sys.exit(f"unknown: {args.what} {args.action}")
    answer = send(args.port or find_port(), command, timeout)
    print(answer)
    sys.exit(0 if answer.splitlines()[-1].startswith("OK") else 1)


if __name__ == "__main__":
    main()
