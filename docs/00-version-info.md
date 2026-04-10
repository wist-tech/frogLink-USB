# Version Information & Platform Support

## Tested Environment

This project was developed and tested on the following system:

| Component | Version |
|---|---|
| OS | Ubuntu 24.04.4 LTS |
| Kernel | 6.17.0-20-generic |
| Python | 3.12.3 |
| pyserial | 3.5 |
| GCC | 13.3.0 |
| frogware (device firmware) | 1.9.11.3 |
| frogLink API | 2.0 |
| frogblue ProjectApp | 1.10.1 |

## Platform Support

### Python Interface (`src/froglink.py`)

| Platform | Detection | Serial I/O | Status |
|---|---|---|---|
| Linux | Full (udev, by-id symlinks) | pyserial | Tested |
| macOS | Supported (`/dev/tty.usbserial-*`) | pyserial | Supported |
| Windows | Supported (`serial.tools.list_ports`) | pyserial | Supported (untested) |

The Python interface is cross-platform. Device detection uses platform-specific methods automatically, while serial I/O is handled by `pyserial` on all platforms.

### C Interface (`src/c/`)

| Platform | Detection | Serial I/O | Threading | Status |
|---|---|---|---|---|
| Linux | Full (glob, udev) | termios | pthreads | Tested |
| macOS | Supported (glob) | termios | pthreads | Supported |
| Windows | Not supported | Not supported | Not supported | Not supported |

The C interface uses POSIX APIs (termios, pthreads, select, glob) and is compatible with Linux and macOS. A Windows port would require Win32 API equivalents (CreateFile, SetCommState, CreateThread, etc.) and is not currently implemented.

## frogware Compatibility

The frogLink API version 2.0 was introduced with frogware 1.8.x. This project has been tested with frogware 1.9.11.3. Earlier versions may have limited functionality:

| frogware Version | API Version | Notes |
|---|---|---|
| 1.7.3.x | undefined | Initial text mode support |
| 1.8.x | 2.0 | JSON format, sensor messages, heating control |
| 1.9.11.3 | 2.0 | Current tested version |

## Checking Versions

### frogware Version

Send the project query command over serial:

```json
{"cmd":"project"}
```

The `swVersion` field shows the installed frogware version.

### frogblue ProjectApp Version

Check the app's settings or about screen on your mobile device.

### System Versions (Linux)

```bash
lsb_release -ds          # Ubuntu version
uname -r                 # Kernel version
python3 --version         # Python version
python3 -c "import serial; print(serial.VERSION)"  # pyserial version
gcc --version | head -1   # GCC version
```
