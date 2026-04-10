#!/usr/bin/env python3
"""
interactive.py — Interactive frogLink console.

Connects to the frogLink, starts a background listener for incoming
messages, and provides a command prompt for sending commands.
"""

import json
import sys
import os
import time

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))

from froglink import FrogLink, FrogLinkError, NotProvisionedError


def on_message(msg):
    """Called for each incoming message from the frogblue network."""
    if "_raw" in msg:
        print(f"\n  << {msg['_raw']}")
    elif msg.get("newMsg"):
        name = msg["newMsg"]
        msg_type = msg.get("type", "control")
        source = msg.get("source", "?")
        values = msg.get("values", {})
        parts = []
        for k, v in values.items():
            if isinstance(v, dict):
                val = v.get("value", "?")
                unit = v.get("unit", "")
                parts.append(f"{k}={val}{unit or ''}")
            else:
                parts.append(f"{k}={v}")
        detail = ", ".join(parts) if parts else ""
        print(f"\n  << [{msg_type}] {name} from {source}: {detail}")
    elif msg.get("type") == "sStatus":
        source = msg.get("source", "?")
        output = msg.get("output", "")
        values = msg.get("values", {})
        parts = []
        for k, v in values.items():
            if isinstance(v, dict):
                val = v.get("value", "?")
                unit = v.get("unit", "")
                parts.append(f"{k}={val}{unit or ''}")
            else:
                parts.append(f"{k}={v}")
        detail = ", ".join(parts)
        out_str = f" output {output}" if output else ""
        print(f"\n  << [status] device {source}{out_str}: {detail}")
    else:
        print(f"\n  << {json.dumps(msg)}")
    print("frogLink> ", end="", flush=True)


def print_help():
    print("""
Commands:
  <message>              Toggle a message (e.g., "Desk Lamp")
  <message> on           Switch on
  <message> off          Switch off
  <message> bright <N>   Set brightness (0-100)
  <message> time <Xm>    Timed operation (e.g., 5m, 30s)
  info                   Show project info
  messages               List configured messages
  rooms                  List configured rooms
  types                  List configured types
  status <addr> <output> Query device output (e.g., status 0822 A)
  raw <json>             Send raw JSON command
  help                   Show this help
  quit                   Exit
""")


def parse_and_send(link, line):
    """Parse user input and send the appropriate command."""
    parts = line.strip().split()
    if not parts:
        return

    cmd = parts[0].lower()

    if cmd == "help":
        print_help()
        return
    if cmd in ("quit", "exit", "q"):
        raise KeyboardInterrupt
    if cmd == "info":
        info = link.project()
        for k, v in info.items():
            print(f"  {k}: {v}")
        return
    if cmd == "messages":
        for m in link.messages():
            print(f"  - {m}")
        return
    if cmd == "rooms":
        for r in link.rooms():
            print(f"  - {r}")
        return
    if cmd == "types":
        for t in link.types():
            print(f"  - {t}")
        return
    if cmd == "status" and len(parts) >= 3:
        results = link.query_output(parts[1], parts[2])
        for r in results:
            print(f"  {json.dumps(r)}")
        return
    if cmd == "raw":
        raw_json = " ".join(parts[1:])
        try:
            obj = json.loads(raw_json)
        except json.JSONDecodeError:
            print("  Invalid JSON")
            return
        results = link._send_json(obj)
        for r in results:
            print(f"  {json.dumps(r)}")
        return

    # Otherwise treat as a message name, possibly with parameters
    # Find the message name — could be multi-word, match against known messages
    available = link.messages()
    msg_name = None
    remaining = []

    # Try matching longest prefix first
    for i in range(len(parts), 0, -1):
        candidate = " ".join(parts[:i])
        if candidate in available:
            msg_name = candidate
            remaining = parts[i:]
            break

    if not msg_name:
        # Assume first word is the message name
        msg_name = parts[0]
        remaining = parts[1:]

    kwargs = {}
    i = 0
    while i < len(remaining):
        token = remaining[i].lower()
        if token == "on":
            kwargs["on"] = True
        elif token == "off":
            kwargs["on"] = False
        elif token == "bright" and i + 1 < len(remaining):
            i += 1
            kwargs["bright"] = int(remaining[i])
        elif token == "time" and i + 1 < len(remaining):
            i += 1
            kwargs["time"] = remaining[i]
        elif token == "pos" and i + 1 < len(remaining):
            i += 1
            kwargs["pos"] = int(remaining[i])
        elif token == "slats" and i + 1 < len(remaining):
            i += 1
            kwargs["slats"] = int(remaining[i])
        i += 1

    print(f"  Sending: {msg_name} {kwargs if kwargs else '(toggle)'}")
    results = link.send(msg_name, **kwargs)
    for r in results:
        if "_raw" in r:
            print(f"  >> {r['_raw']}")
        else:
            print(f"  >> {json.dumps(r)}")


def main():
    link = FrogLink()

    try:
        info = link.connect()
    except NotProvisionedError:
        print("frogLink is not provisioned. Run detect.py for details.")
        sys.exit(1)
    except FrogLinkError as e:
        print(f"Error: {e}")
        sys.exit(1)

    print(f"Connected to project: {info.get('project', '?')}")
    print(f"Device: {info.get('frogLinkName', '?')} in {info.get('frogLinkRoom', '?')}")
    print(f"Frogware: {info.get('swVersion', '?')} | API: {info.get('api', '?')}")
    print()

    messages = link.messages()
    print(f"Available messages: {', '.join(messages) if messages else '(none)'}")
    print()

    # Enable receiving and start listener
    link.enable_messages(True)
    link.enable_status(True)
    link.start_listener(on_message)
    print("Listener started — incoming messages will appear below.")
    print("Type 'help' for commands.\n")

    try:
        while True:
            try:
                line = input("frogLink> ")
            except EOFError:
                break
            if not line.strip():
                continue
            try:
                parse_and_send(link, line)
            except FrogLinkError as e:
                print(f"  Error: {e}")
            except Exception as e:
                print(f"  Error: {e}")
    except KeyboardInterrupt:
        print("\nDisconnecting...")

    link.disconnect()
    print("Bye.")


if __name__ == "__main__":
    main()
