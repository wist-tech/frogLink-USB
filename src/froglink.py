"""
froglink.py — Python interface for the frogLink USB gateway.

Communicates with frogblue devices via serial JSON commands.
"""

import json
import threading
import time
import glob
import subprocess
from typing import Optional

import serial


class FrogLinkError(Exception):
    """Base exception for frogLink errors."""


class NotProvisionedError(FrogLinkError):
    """Raised when the frogLink has no project configuration."""


class FrogLink:
    """Interface to a frogLink USB gateway.

    Uses JSON format for all communication. Provides methods for querying
    device info, sending control messages, and listening for incoming
    messages from the frogblue network.

    Usage:
        link = FrogLink()          # auto-detect port
        link = FrogLink("/dev/ttyUSB0")  # explicit port
        link.connect()
        info = link.project()
        link.send("Desk Lamp")
        link.disconnect()
    """

    BAUD_RATE = 115200
    SERIAL_TIMEOUT = 3
    READ_DELAY = 1.5  # seconds to wait for response

    def __init__(self, port: Optional[str] = None):
        self.port_path = port
        self._serial: Optional[serial.Serial] = None
        self._listener_thread: Optional[threading.Thread] = None
        self._listening = False
        self._on_message = None
        self._lock = threading.Lock()

    # -- Detection ----------------------------------------------------------

    @staticmethod
    def detect() -> Optional[str]:
        """Auto-detect a frogLink USB device.

        Checks /dev/serial/by-id/ for frogblue devices (Linux),
        falls back to /dev/ttyUSB* and /dev/tty.usbserial-* patterns.

        Returns the port path, or None if not found.
        """
        # Linux: check by-id symlinks (most reliable)
        by_id = glob.glob("/dev/serial/by-id/*frogblue*FrogLink*")
        if by_id:
            return by_id[0]

        # Linux: check udev info on ttyUSB devices
        for path in sorted(glob.glob("/dev/ttyUSB*")):
            try:
                result = subprocess.run(
                    ["udevadm", "info", "--query=property", "--name", path],
                    capture_output=True, text=True, timeout=5,
                )
                if "FrogLink" in result.stdout:
                    return path
            except (subprocess.TimeoutExpired, FileNotFoundError):
                continue

        # macOS
        for path in sorted(glob.glob("/dev/tty.usbserial-*")):
            return path  # best guess on macOS

        return None

    # -- Connection ---------------------------------------------------------

    def connect(self) -> dict:
        """Open the serial connection and query project info.

        Auto-detects the port if none was specified.

        Returns:
            Project info dict if provisioned.

        Raises:
            FrogLinkError: If no frogLink is found.
            NotProvisionedError: If the device has no configuration.
        """
        if self._serial and self._serial.is_open:
            return self.project()

        if not self.port_path:
            self.port_path = self.detect()
            if not self.port_path:
                raise FrogLinkError(
                    "No frogLink USB device found. "
                    "Check that it is plugged in and you have permission to access serial ports."
                )

        self._serial = serial.Serial(
            self.port_path,
            self.BAUD_RATE,
            timeout=self.SERIAL_TIMEOUT,
            bytesize=8,
            parity="N",
            stopbits=1,
            xonxoff=False,
            rtscts=False,
            dsrdtr=False,
        )
        time.sleep(0.5)
        self._serial.reset_input_buffer()

        return self.project()

    def disconnect(self):
        """Stop listener and close the serial connection."""
        self.stop_listener()
        if self._serial and self._serial.is_open:
            self._serial.close()
        self._serial = None

    @property
    def connected(self) -> bool:
        return self._serial is not None and self._serial.is_open

    # -- Low-level I/O ------------------------------------------------------

    def _send_raw(self, data: str) -> str:
        """Send a string and return the raw response."""
        if not self.connected:
            raise FrogLinkError("Not connected.")

        with self._lock:
            self._serial.reset_input_buffer()
            self._serial.write((data + "\n").encode("ascii"))
            time.sleep(self.READ_DELAY)

            response = b""
            while self._serial.in_waiting:
                response += self._serial.read(self._serial.in_waiting)
                time.sleep(0.1)

        return response.decode("ascii", errors="replace").strip()

    def _send_json(self, obj: dict) -> list[dict]:
        """Send a JSON command and return parsed response(s).

        Returns a list because some commands produce multiple JSON lines.
        """
        raw = self._send_raw(json.dumps(obj, separators=(",", ":")))

        results = []
        for line in raw.split("\n"):
            line = line.strip()
            if not line:
                continue
            try:
                results.append(json.loads(line))
            except json.JSONDecodeError:
                results.append({"_raw": line})

        if len(results) == 1 and "err" in results[0]:
            err = results[0]["err"]
            if err == "no config":
                raise NotProvisionedError(
                    "frogLink has no configuration. "
                    "Provision it using the frogblue ProjectApp first."
                )
            raise FrogLinkError(f"Device error: {err}")

        return results

    # -- Query commands -----------------------------------------------------

    def project(self) -> dict:
        """Request project information."""
        results = self._send_json({"cmd": "project"})
        return results[0] if results else {}

    def messages(self) -> list[str]:
        """Request the list of configured control messages."""
        results = self._send_json({"cmd": "messages"})
        return results[0].get("message", []) if results else []

    def rooms(self) -> list[str]:
        """Request the list of configured rooms."""
        results = self._send_json({"cmd": "rooms"})
        return results[0].get("rooms", []) if results else []

    def types(self, room: Optional[str] = None) -> list[str]:
        """Request the list of configured types, optionally filtered by room."""
        cmd = {"cmd": "types"}
        if room:
            cmd["room"] = room
        results = self._send_json(cmd)
        return results[0].get("types", []) if results else []

    def is_provisioned(self) -> bool:
        """Check whether the frogLink has a project configuration."""
        try:
            self.project()
            return True
        except NotProvisionedError:
            return False

    # -- Message enable/disable ---------------------------------------------

    def enable_messages(self, enabled: bool = True) -> dict:
        """Enable or disable receiving control messages (JSON)."""
        results = self._send_json({"cmd": "msgenable", "enabled": enabled})
        return results[0] if results else {}

    def enable_status(self, enabled: bool = True) -> dict:
        """Enable or disable receiving status messages (JSON)."""
        results = self._send_json({"cmd": "statusenable", "enabled": enabled})
        return results[0] if results else {}

    # -- Send control messages ----------------------------------------------

    def send(self, message: str, **kwargs) -> list[dict]:
        """Send a control message.

        Args:
            message: The message name (e.g., "Desk Lamp").
            **kwargs: Optional parameters:
                on (bool): Force on (True) or off (False).
                bright (int): Brightness 0-100.
                time (str): Duration, e.g. "5m", "30s", "1h".
                pos (int): Shutter position 0-100.
                slats (int): Slat position 0-100.

        Returns:
            List of response dicts from the frogLink.
        """
        cmd = {"msg": message}
        cmd.update(kwargs)
        return self._send_json(cmd)

    def send_status(self, name: str, status: bool) -> list[dict]:
        """Send a control status message (logical gate).

        Args:
            name: Status name (e.g., "Night").
            status: True or False.
        """
        return self._send_json({"msg": name, "status": status, "changed": True})

    def query_output(self, target: str, output: str = "A") -> list[dict]:
        """Request the current output state of a device.

        Args:
            target: Device network address as hex string (e.g., "0822").
            output: Output channel letter (e.g., "A", "B").

        Requires status messages to be enabled.
        """
        return self._send_json({
            "cmd": "status",
            "type": "bright",
            "target": target,
            "output": output,
        })

    # -- Background listener ------------------------------------------------

    def start_listener(self, callback):
        """Start a background thread that listens for incoming messages.

        Args:
            callback: Function called with each parsed message dict.
                      Signature: callback(message: dict)
        """
        if self._listening:
            return

        self._on_message = callback
        self._listening = True
        self._listener_thread = threading.Thread(
            target=self._listener_loop, daemon=True
        )
        self._listener_thread.start()

    def stop_listener(self):
        """Stop the background listener thread."""
        self._listening = False
        if self._listener_thread:
            self._listener_thread.join(timeout=3)
            self._listener_thread = None

    def _listener_loop(self):
        """Read incoming serial data and dispatch parsed messages."""
        buffer = ""
        while self._listening and self.connected:
            try:
                if self._serial.in_waiting:
                    with self._lock:
                        chunk = self._serial.read(self._serial.in_waiting)
                    buffer += chunk.decode("ascii", errors="replace")

                    while "\n" in buffer:
                        line, buffer = buffer.split("\n", 1)
                        line = line.strip()
                        if not line:
                            continue
                        try:
                            msg = json.loads(line)
                        except json.JSONDecodeError:
                            msg = {"_raw": line}

                        if self._on_message:
                            try:
                                self._on_message(msg)
                            except Exception:
                                pass  # don't crash listener on callback errors
                else:
                    time.sleep(0.1)
            except (serial.SerialException, OSError):
                break
