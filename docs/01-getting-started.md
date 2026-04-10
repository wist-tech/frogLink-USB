# Getting Started with frogLink USB

For tested versions and platform support details, see [Version Info](00-version-info.md).

## What is frogLink?

The frogLink USB is a serial USB-to-Bluetooth gateway by frogblue AG. It bridges a host computer to frogblue's BLE mesh network, allowing you to send and receive commands to frogblue devices (lights, shutters, sensors, heating) from any system with a USB port.

The frogLink operates in **Text Mode** — all communication is via ASCII characters over a serial connection, using either plain text or JSON format.

## Physical Connection

1. Plug the frogLink USB stick into any available USB port on your computer.
2. The device uses an **FTDI FT232** USB-to-serial chip and will appear as a serial port immediately — no additional drivers are needed on most Linux distributions and macOS. Windows may require the [FTDI VCP driver](https://ftdichip.com/drivers/vcp-drivers/).

## Detecting the frogLink

### Linux

The frogLink appears as `/dev/ttyUSBx` (typically `/dev/ttyUSB0`).

**Find the port:**

```bash
ls /dev/ttyUSB*
```

**Confirm it's a frogLink using udev:**

```bash
udevadm info --query=all --name=/dev/ttyUSB0 | grep -i frog
```

You should see output containing:

```
E: ID_MODEL=FrogLink
E: ID_VENDOR=frogblue_TECHNOLOGY_GmbH
E: ID_SERIAL=frogblue_TECHNOLOGY_GmbH_FrogLink_<SERIAL>
```

**Stable path (recommended for scripts):**

The device is also available at a persistent symlink:

```
/dev/serial/by-id/usb-frogblue_TECHNOLOGY_GmbH_FrogLink_<SERIAL>-if00-port0
```

This path does not change when other USB devices are plugged/unplugged.

### macOS

The frogLink appears as `/dev/tty.usbserial-*`. List serial ports:

```bash
ls /dev/tty.usbserial-*
```

### Windows

The frogLink appears as a COM port (e.g., `COM3`). Check Device Manager under "Ports (COM & LPT)" for "USB Serial Port".

## Permissions (Linux)

The serial port is owned by the `dialout` group. Your user must be a member:

```bash
sudo usermod -aG dialout $USER
```

Log out and back in for the change to take effect. For immediate access without re-login:

```bash
sudo chmod 666 /dev/ttyUSB0
```

## Serial Interface Settings

| Setting      | Value            |
|--------------|------------------|
| Data Rate    | 115200 Baud      |
| Data Bits    | 8                |
| Parity       | None             |
| Stop Bits    | 1                |
| Flow Control | Off              |
| Echo         | Off              |
| End of Line  | LineFeed (`\n`)  |

## Firmware Update

Before using the frogLink for the first time, ensure it is running the latest frogware version. Firmware updates are performed via the **frogblue ProjectApp**:

1. Open the frogblue ProjectApp and connect to your frogblue project.
2. Navigate to the device manager and locate your frogLink.
3. If a firmware update is available, the app will indicate this. Follow the prompts to update.
4. Wait for the update to complete — the frogLink will reboot automatically.

You can verify the installed version at any time by sending the `$project` command (plain text) or `{"cmd":"project"}` (JSON). The `swVersion` field shows the current frogware version. At the time of writing, the latest version is **1.9.11.3**.

## First Communication

You can test the connection using any serial terminal (e.g., `screen`, `minicom`, `picocom`) or with the included Python and C examples.

### Using screen

```bash
screen /dev/ttyUSB0 115200
```

Type `$project` and press Enter. You will receive either:

- **Unprovisioned device:** `$no config`
- **Provisioned device:** Project information including name, firmware version, MAC address, etc.

Exit screen with `Ctrl-A` then `\`.

### Using the Python detect example

```bash
pip install pyserial
python3 src/examples/detect.py
```

### Using the C detect example

```bash
cd src/c
make
./examples/detect
```

### Using Python directly

```python
import serial, time

port = serial.Serial('/dev/ttyUSB0', 115200, timeout=3)
time.sleep(0.5)
port.reset_input_buffer()

port.write(b'$project\n')
time.sleep(1.5)

response = port.read(port.in_waiting).decode('ascii', errors='replace')
print(response)

port.close()
```

## Understanding Device State

### Factory / Unprovisioned

A brand-new or factory-reset frogLink has no project configuration. Every command returns:

| Format     | Response             |
|------------|----------------------|
| Plain text | `$no config`         |
| JSON       | `{"err":"no config"}`|

The device is functional (serial communication works) but has no messages, rooms, or types configured. It must be provisioned using the frogblue ProjectApp before it can control devices. See [Provisioning Guide](02-provisioning.md).

### Provisioned

A provisioned frogLink has been added to a frogblue project and configured with messages, rooms, and types. The `$project` command returns full project details.

**Plain text response:**

```
$
project=WIST Camper
frogLinkName= Camper USB
frogLinkRoom=Camper Control
frogLinkBuilding=
swVersion=1.9.11.3
config=10.04.2026 15:54:09
address=A8:36:7A:00:4E:AC
netId=80
$
```

**JSON response:**

```json
{
  "project": "WIST Camper",
  "frogLinkName": " Camper USB",
  "frogLinkRoom": "Camper Control",
  "frogLinkBuilding": "",
  "swVersion": "1.9.11.3",
  "config": "10.04.2026 15:54:09",
  "address": "A8:36:7A:00:4E:AC",
  "nadd": "084C",
  "netId": "80",
  "api": "2.0"
}
```

| Field | Description |
|---|---|
| `project` | Name of the frogblue project |
| `frogLinkName` | Device name assigned in ProjectApp |
| `frogLinkRoom` | Room assignment in ProjectApp |
| `frogLinkBuilding` | Building assignment (optional) |
| `swVersion` | Installed frogware version |
| `config` | Date and time of last configuration |
| `address` | Bluetooth MAC address |
| `nadd` | Device network address (hex) |
| `netId` | Network ID of the project |
| `api` | API version supported |

## Example Interfaces

This project includes two complete example interfaces — **Python** and **C** — both verified against real hardware.

### Python (`src/`)

Requires `pyserial`. Install with `pip install pyserial`.

| Script | Description |
|---|---|
| `src/examples/detect.py` | Detect frogLink and display state |
| `src/examples/listen.py` | Monitor all incoming messages |
| `src/examples/interactive.py` | Interactive command console |
| `src/froglink.py` | Core library (import in your own projects) |

### C (`src/c/`)

No external dependencies — uses POSIX termios and pthreads.

```bash
cd src/c && make
```

| Binary | Description |
|---|---|
| `examples/detect` | Detect frogLink and display state |
| `examples/listen` | Monitor all incoming messages |
| `examples/interactive` | Interactive command console |
| `froglink.h` / `froglink.c` | Core library (link in your own projects) |

See [Provisioning Guide](02-provisioning.md) for how to set up your frogLink, and [Command Reference](03-command-reference.md) for the full command set.
