#!/usr/bin/env python3
"""
detect.py — Detect a frogLink USB device and report its state.

Finds the frogLink, connects, and displays whether it is provisioned
or in factory/unprovisioned state. If provisioned, shows project info
and available messages, rooms, and types.
"""

import sys
import os

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))

from froglink import FrogLink, FrogLinkError, NotProvisionedError


def main():
    print("Searching for frogLink USB device...")

    port = FrogLink.detect()
    if not port:
        print("No frogLink USB device found.")
        print("Check that it is plugged in and you have permission to access serial ports.")
        print("  Linux: sudo usermod -aG dialout $USER  (then re-login)")
        sys.exit(1)

    print(f"Found frogLink at: {port}")
    print()

    link = FrogLink(port)

    try:
        info = link.connect()
    except NotProvisionedError:
        print("Status: UNPROVISIONED (factory state)")
        print()
        print("The frogLink has no project configuration.")
        print("To provision it:")
        print("  1. Open the frogblue ProjectApp")
        print("  2. Add the frogLink to your project")
        print("  3. Assign messages, rooms, and types")
        print("  4. Deploy the configuration")
        print()
        print("Run this script again after provisioning.")
        link.disconnect()
        sys.exit(0)

    print("Status: PROVISIONED")
    print()
    print("Project Information:")
    print(f"  Project:     {info.get('project', '?')}")
    print(f"  Device Name: {info.get('frogLinkName', '?')}")
    print(f"  Room:        {info.get('frogLinkRoom', '?')}")
    print(f"  Frogware:    {info.get('swVersion', '?')}")
    print(f"  Config Date: {info.get('config', '?')}")
    print(f"  MAC Address: {info.get('address', '?')}")
    print(f"  Network Addr:{info.get('nadd', '?')}")
    print(f"  Net ID:      {info.get('netId', '?')}")
    print(f"  API Version: {info.get('api', '?')}")
    print()

    messages = link.messages()
    print(f"Messages ({len(messages)}):")
    if messages:
        for m in messages:
            print(f"  - {m}")
    else:
        print("  (none configured)")
    print()

    rooms = link.rooms()
    print(f"Rooms ({len(rooms)}):")
    if rooms:
        for r in rooms:
            print(f"  - {r}")
    else:
        print("  (none configured)")
    print()

    types = link.types()
    print(f"Types ({len(types)}):")
    if types:
        for t in types:
            print(f"  - {t}")
    else:
        print("  (none configured)")

    link.disconnect()


if __name__ == "__main__":
    main()
