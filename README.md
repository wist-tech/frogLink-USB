# frogLink-USB

Python and C interfaces for the **frogblue frogLink USB** gateway — a serial USB-to-Bluetooth bridge for the frogblue smart home system.

The frogLink USB stick allows any computer to send and receive commands to frogblue devices (lights, shutters, sensors, heating) over the frogblue BLE mesh network using simple ASCII commands via a serial connection.

## Features

- **Auto-detection** of frogLink USB devices
- **Factory state recognition** — detect unprovisioned devices and guide setup
- **JSON command interface** — send control messages, query status, manage configuration
- **Background listener** — receive incoming messages from the frogblue network in real-time
- **Python and C implementations** — identical functionality, no external dependencies for C

## Quick Start

### Python

```bash
pip install pyserial
python3 src/examples/detect.py
```

### C

```bash
cd src/c
make
./examples/detect
```

### Example Output

```
Searching for frogLink USB device...
Found frogLink at: /dev/serial/by-id/usb-frogblue_TECHNOLOGY_GmbH_FrogLink_...-if00-port0

Status: PROVISIONED

Project Information:
  Project:      WIST Camper
  Device Name:  Camper USB
  Room:         Camper Control
  Frogware:     1.9.11.3

Messages (4):
  - Central_ON
  - Central_OFF
  - CamperDoor
  - Desk Lamp
```

## Project Structure

```
├── docs/
│   ├── 01-getting-started.md      # Connection, detection, firmware update
│   ├── 02-provisioning.md         # frogblue ProjectApp setup guide
│   └── 03-command-reference.md    # Full command reference (plain text + JSON)
├── src/
│   ├── froglink.py                # Python library
│   ├── examples/
│   │   ├── detect.py              # Detect device & display state
│   │   ├── listen.py              # Monitor all incoming messages
│   │   └── interactive.py         # Interactive command console
│   └── c/
│       ├── froglink.h             # C library header
│       ├── froglink.c             # C library implementation
│       ├── Makefile
│       └── examples/
│           ├── detect.c           # Detect device & display state
│           ├── listen.c           # Monitor all incoming messages
│           └── interactive.c      # Interactive command console
└── requirements.txt               # Python dependencies (pyserial)
```

## Example Programs

| Program | Description |
|---|---|
| `detect` | Find the frogLink, check if provisioned, display project info and configured messages/rooms/types |
| `listen` | Enable message and status receiving, print all incoming messages with timestamps |
| `interactive` | Full command console — send messages, query devices, monitor incoming traffic |

### Interactive Console Commands

```
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
help                   Show help
quit                   Exit
```

## Serial Settings

| Setting | Value |
|---|---|
| Baud Rate | 115200 |
| Data Bits | 8 |
| Parity | None |
| Stop Bits | 1 |
| Flow Control | Off |
| Line Ending | LF (`\n`) |

## Platform Support

| | Linux | macOS | Windows |
|---|---|---|---|
| **Python** | Tested | Supported | Supported (untested) |
| **C** | Tested | Supported (POSIX) | Not supported |

The Python interface is cross-platform — device detection adapts automatically per OS, and `pyserial` handles serial I/O on all platforms. The C interface uses POSIX APIs (termios, pthreads) and is compatible with Linux and macOS.

## Tested Environment

| Component | Version |
|---|---|
| Ubuntu | 24.04.4 LTS |
| Python | 3.12.3 / pyserial 3.5 |
| GCC | 13.3.0 |
| frogware | 1.9.11.3 |
| frogLink API | 2.0 |
| frogblue ProjectApp | 1.10.1 |

## Prerequisites

- **frogLink USB** stick by frogblue AG
- **frogblue ProjectApp** (iOS/Android) for provisioning — version 1.10.1 or later recommended
- Linux or macOS for C interface; any OS for Python
- Linux users: add yourself to the `dialout` group (`sudo usermod -aG dialout $USER`)

## Documentation

See the [docs/](docs/) directory for detailed guides:

0. [Version Info](docs/00-version-info.md) — tested versions and platform support
1. [Getting Started](docs/01-getting-started.md) — physical connection, detection, first communication
2. [Provisioning](docs/02-provisioning.md) — setting up frogLink in the frogblue ProjectApp
3. [Command Reference](docs/03-command-reference.md) — complete command reference with examples

## License

MIT
