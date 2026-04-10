#!/usr/bin/env python3
"""
listen.py — Listen for all incoming frogLink messages.

Connects to the frogLink, enables message and status receiving,
and prints every incoming message until interrupted with Ctrl+C.
Useful for monitoring network activity and debugging.
"""

import json
import sys
import os
import time

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))

from froglink import FrogLink, FrogLinkError, NotProvisionedError


def on_message(msg):
    """Print each message with a timestamp."""
    ts = time.strftime("%H:%M:%S")
    print(f"[{ts}] {json.dumps(msg)}")


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

    print(f"Connected: {info.get('project', '?')} | {info.get('frogLinkName', '?')}")
    print(f"Frogware {info.get('swVersion', '?')} | API {info.get('api', '?')}")
    print()

    link.enable_messages(True)
    link.enable_status(True)
    link.start_listener(on_message)

    print("Listening for messages (Ctrl+C to stop)...")
    print()

    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        print("\nStopping...")

    link.disconnect()


if __name__ == "__main__":
    main()
