# frogLink Command Reference

The frogLink supports two command formats: **plain text** and **JSON**. Both can be used simultaneously, though it is recommended to choose one. This reference covers both formats side by side.

All commands are terminated with a LineFeed (`\n`) character.

---

## 1. General Commands

### 1.1 Request Project Information

Returns project name, device info, firmware version, and network details.

**Plain text:**

```
$project
```

Response:

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

**JSON:**

```json
{"cmd":"project"}
```

Response:

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

**Unprovisioned device** returns `$no config` (plain text) or `{"err":"no config"}` (JSON).

---

### 1.2 Request Available Messages

Returns the list of control messages configured for this frogLink.

**Plain text:** `$message` or `$messages`

**JSON:** `{"cmd":"messages"}` or `{"cmd":"message"}`

Example response:

```json
{"message":["Central_ON","Central_OFF","Desk Lamp"]}
```

---

### 1.3 Request Available Rooms

Returns the list of rooms that can be addressed with type messages.

**Plain text:** `$room` or `$rooms`

**JSON:** `{"cmd":"rooms"}` or `{"cmd":"room"}`

Example response:

```json
{"rooms":["Camper / Camper Inside","Camper / Camper Control"]}
```

---

### 1.4 Request Available Types

Returns the list of type messages (broadcast by room) configured for this frogLink.

**Plain text:** `$type` or `$types`

**JSON:** `{"cmd":"types"}`

Example response:

```json
{"types":["Open door"]}
```

---

### 1.5 Combine Requests

Filter types by room.

**Plain text:** `$types(room=Camper / Camper Inside)`

**JSON:** `{"cmd":"types","room":"Camper / Camper Inside"}`

---

### 1.6 Enable/Disable Receiving Messages

Controls whether control messages from the frogblue network are forwarded over serial. **Enabled by default.** Plain text and JSON receiving are independent settings.

| Action  | Plain Text     | JSON                                  |
|---------|----------------|---------------------------------------|
| Enable  | `$msgenable`   | `{"cmd":"msgenable","enabled":true}`  |
| Disable | `$msgdisable`  | `{"cmd":"msgenable","enabled":false}` |

---

### 1.7 Enable/Disable Receiving Status Messages

Controls whether standard and control status messages are forwarded. **Disabled by default.** Must be enabled to receive device output states, sensor data, and heating status.

| Action  | Plain Text        | JSON                                     |
|---------|-------------------|------------------------------------------|
| Enable  | `$statusenable`   | `{"cmd":"statusenable","enabled":true}`  |
| Disable | `$statusdisable`  | `{"cmd":"statusenable","enabled":false}` |

---

### 1.8 Set Baud Rate

Change the serial baud rate (range: 1200–115200). The device reboots after this command.

**Plain text:** `$asc(9600)`

**JSON:** `{"cmd":"asc","baud":9600}`

---

## 2. Sending Control Messages

Control messages trigger actions on frogblue devices. The message name must match one configured on the frogLink.

### 2.1 Basic Control (Lights, Relays)

**Available parameters:**

| Parameter | Plain Text | JSON | Description |
|-----------|-----------|------|-------------|
| Toggle | `MessageName` | `{"msg":"MessageName"}` | Toggle output on/off |
| On | `MessageName(ON)` | `{"msg":"MessageName","on":true}` | Force on |
| Off | `MessageName(OFF)` | `{"msg":"MessageName","on":false}` | Force off |
| Timed | `MessageName(ON,time=5m)` | `{"msg":"MessageName","on":true,"time":"5m"}` | On for duration |
| Dim | `MessageName(bright=40)` | `{"msg":"MessageName","bright":40}` | Set brightness 0–100 |
| Combined | `MessageName(ON,time=5m,bright=40)` | `{"msg":"MessageName","on":true,"time":"5m","bright":40}` | All parameters |

Time units: `s` (seconds), `m` (minutes), `h` (hours).

**Example — Toggle "Desk Lamp":**

```json
{"msg":"Desk Lamp"}
```

Response:

```json
{"newMsg":"Desk Lamp","p0":255,"p1":255,"p2":0}
```

The response confirms the message was sent. The `p0`/`p1`/`p2` values are the raw parameters transmitted — they do **not** reflect the current output state.

| Response Field | Meaning |
|---|---|
| `p0` | Brightness value (255 = last used, 0–100 = specific %) |
| `p1` | Reserved (255 = default) |
| `p2` | Timer value (0 = none, e.g., "5m" = 5 minutes) |

When using `on:true` or `on:false`, the response message name has `-Value` appended (e.g., `Desk Lamp-Value`), indicating an absolute command rather than a toggle.

---

### 2.2 Shutter Control

Shutters require **separate messages** for down, up, and position. Each must be configured individually in the ProjectApp.

| Action | Plain Text | JSON |
|---|---|---|
| Down/Stop | `ShutterName` | `{"msg":"ShutterName"}` |
| Up/Stop | `ShutterName-Up` | `{"msg":"ShutterName-Up"}` |
| Position | `ShutterName-Pos(pos=30)` | `{"msg":"ShutterName-Pos","pos":30}` |
| Position + Slats | `ShutterName-Pos(pos=30,slats=50)` | `{"msg":"ShutterName-Pos","pos":30,"slats":50}` |
| Slats only | `ShutterName-Pos(slats=80)` | `{"msg":"ShutterName-Pos","slats":80}` |

Position values: 0% = fully open, 100% = fully closed.

---

### 2.3 Request Device Output Status

Query the current output state of a specific device by its network address and output channel. Requires `statusenable` to be set.

**Plain text:**

```
$status(type=bright,target=0x0822,output=A)
```

**JSON:**

```json
{"cmd":"status","type":"bright","target":"0822","output":"A"}
```

Response (JSON, multiple messages):

```json
{"newMsg":null,"type":"sStatus","source":"0822","output":"A","values":{"on":{"value":true,"unit":null}}}
{"newMsg":null,"type":"sStatus","source":"0822","output":"A","values":{"bright":{"value":52,"unit":"%"}}}
{"newMsg":null,"type":"sStatus","source":"0822","output":"A","values":{"power":{"value":23,"unit":"W"}}}
```

---

### 2.4 Control Status Messages (Logical Gates)

Set logical gate states (e.g., Night, Wind) used for conditional behavior in the frogblue system. These must be **repeated every 4–6 minutes** (the gate times out after 8 minutes).

**Plain text:**

```
Night(true)
Night(false)
```

**JSON:**

```json
{"msg":"Night","status":true,"changed":true}
{"msg":"Night","status":false,"changed":true}
```

---

### 2.5 Heating Control (JSON only)

```json
{"cmd":"heat","room":"Office","dT":22.5,"nT":19.0,"day":true,"mode":0,"cool":false,"accept":true,"allow":false}
```

| Parameter | Type | Description |
|---|---|---|
| `room` | string | Target room |
| `dT` | float | Desired day temperature |
| `nT` | float | Desired night temperature |
| `day` | boolean | Day mode active |
| `mode` | integer | 0=none, 1=party, 2=away, 3=vacation |
| `cool` | boolean | Cooling mode |
| `accept` | boolean | Accept values from other devices |
| `allow` | boolean | Night mode on window open |

---

## 3. Receiving Messages

Messages from the frogblue network are forwarded over serial when receiving is enabled. Multiple message types can arrive at any time.

### 3.1 Control Messages

Received when a configured message is triggered by any source on the network (e.g., a physical button press).

**Plain text:**

```
Desk Lamp(255,255,0)
Desk Lamp(100,0,0)
```

**JSON:**

```json
{"newMsg":"Desk Lamp","p0":255,"p1":255,"p2":0}
{"newMsg":"Desk Lamp","p0":100,"p1":255,"p2":0}
```

---

### 3.2 Standard Status Messages

Periodic output state reports from devices (every ~8 minutes and on change). Requires `statusenable`.

**Dim output:**

```json
{"newMsg":null,"type":"sStatus","source":"0822","output":"B","values":{"bright":{"value":0,"unit":"%"}}}
```

**Plain text:**

```
$status(output=B,value=0%,source=0822,dest=FFFF)
```

---

### 3.3 Shutter Status

```json
{"newMsg":null,"type":"sStatus","source":"0101","output":"A","values":{"pos":{"value":40,"unit":"%"},"slats":{"value":100,"unit":"%"}}}
```

---

### 3.4 Sensor Status

Devices with environmental sensors send periodic readings.

```json
{
  "newMsg": null,
  "type": "sStatus",
  "source": "0083",
  "values": {
    "temp":     {"value": 21.4, "unit": "\u00b0C"},
    "humidity": {"value": 31,   "unit": "%"},
    "bright":   {"value": 10,   "unit": "lux"}
  }
}
```

---

### 3.5 Control Status Messages

Logical gate state broadcasts (every 1–8 minutes).

```json
{"newMsg":"Night","type":"cStatus","p0":1,"p1":255,"p2":0,"changed":true}
```

---

### 3.6 Heating Status

Heating device reports (every 4 minutes or on change).

```json
{
  "newMsg": null,
  "type": "sStatus",
  "source": "18A2",
  "channel": 8,
  "values": {
    "temp":    {"value": 22.5, "unit": "C"},
    "day":     {"value": true, "unit": null},
    "cooling": {"value": 0,    "unit": null},
    "mode":    {"value": false, "unit": null}
  }
}
```

---

### 3.7 Water Meter / Pulse Counter

```json
{
  "newMsg": null,
  "type": "sStatus",
  "source": "0421",
  "dest": "FFFF",
  "values": {
    "channel": {"value": 0,    "unit": "channel"},
    "count":   {"value": 4544, "unit": "count"}
  }
}
```

---

## 4. Quick Reference

### Plain Text Commands

| Command | Description |
|---|---|
| `$project` | Project information |
| `$messages` | List configured messages |
| `$rooms` | List configured rooms |
| `$types` | List configured types |
| `$types(room=X)` | Types in a specific room |
| `$msgenable` / `$msgdisable` | Enable/disable message receiving |
| `$statusenable` / `$statusdisable` | Enable/disable status receiving |
| `$asc(X)` | Set baud rate |
| `MessageName` | Toggle |
| `MessageName(ON)` / `MessageName(OFF)` | Force on/off |
| `MessageName(bright=X)` | Set brightness |
| `MessageName(time=Xm)` | Timed operation |
| `$status(type=bright,target=0xWXYZ,output=X)` | Query device output |
| `StatusName(true)` / `StatusName(false)` | Set control status |

### JSON Commands

| Command | Description |
|---|---|
| `{"cmd":"project"}` | Project information |
| `{"cmd":"messages"}` | List configured messages |
| `{"cmd":"rooms"}` | List configured rooms |
| `{"cmd":"types"}` | List configured types |
| `{"cmd":"types","room":"X"}` | Types in a specific room |
| `{"cmd":"msgenable","enabled":true/false}` | Enable/disable message receiving |
| `{"cmd":"statusenable","enabled":true/false}` | Enable/disable status receiving |
| `{"cmd":"asc","baud":X}` | Set baud rate |
| `{"msg":"Name"}` | Toggle |
| `{"msg":"Name","on":true/false}` | Force on/off |
| `{"msg":"Name","bright":X}` | Set brightness |
| `{"msg":"Name","time":"Xm"}` | Timed operation |
| `{"msg":"Name","pos":X,"slats":X}` | Shutter position |
| `{"cmd":"status","type":"bright","target":"WXYZ","output":"X"}` | Query device output |
| `{"msg":"Name","status":true/false,"changed":true}` | Set control status |
| `{"cmd":"heat","room":"X",...}` | Heating control |
