# Provisioning the frogLink USB

A new or factory-reset frogLink must be provisioned before it can send and receive commands on the frogblue network. Provisioning is done using the **frogblue ProjectApp** (available for iOS and Android).

## Prerequisites

- A frogblue project with at least one configured device (e.g., a frogDim, frogRelay, frogMultiSense, etc.)
- The frogblue ProjectApp installed on an iPad or Android tablet.
- The frogLink USB plugged into a computer (it uses Bluetooth to communicate with the ProjectApp during configuration)

## Step 1: Update Firmware

Before provisioning, ensure the frogLink is running the latest frogware:

1. Open the frogblue ProjectApp and connect to your project.
2. Navigate to the **Device Manager**.
3. The ProjectApp will detect the frogLink via Bluetooth. If a firmware update is available, follow the prompts to update.
4. The frogLink reboots automatically after the update.

You can verify the installed version over serial by sending `$project` or `{"cmd":"project"}` and checking the `swVersion` field.

## Step 2: Add frogLink to the Project

1. In the ProjectApp, go to the **Device Manager**.
2. Add the frogLink to your project by assigning it to a room (e.g., "Camper Control").
3. Optionally give it a descriptive name (e.g., "Camper USB").

After this step, the frogLink will report its project assignment but will have no messages, rooms, or types configured:

```json
{"message":[],"rooms":[],"types":[]}
```

## Step 3: Configure Messages

Messages are the core of frogLink communication. Each message corresponds to a control action (e.g., toggling a light, opening a door). Messages must be explicitly assigned to the frogLink — it can only send and receive messages that have been configured for it.

In the ProjectApp:

1. Navigate to the frogLink's configuration.
2. Add the **control messages** you want the frogLink to be able to send and/or receive.
3. For each message, you can enable **sending**, **receiving**, or both independently.

**Example:** After configuring three messages, the frogLink reports:

```json
{"message":["Central_ON","Central_OFF","Desk Lamp"]}
```

### Message Types

| Type | Description | Example |
|---|---|---|
| Control message | Triggers an action on a device output | `Desk Lamp` (toggle/on/off) |
| Control message (ON-only) | Always switches on | `Central_ON` |
| Control message (OFF-only) | Always switches off | `Central_OFF` |
| Shutter message | Controls a shutter (down/stop) | `ShutterEast` |
| Shutter-Up message | Controls a shutter (up/stop) | `ShutterEast-Up` |
| Shutter-Pos message | Sets shutter/slat position | `ShutterEast-Pos` |

Shutter control requires **separate messages** for down, up, and position — each must be configured individually.

## Step 4: Configure Rooms (Optional)

Rooms allow you to address devices by location using **type messages** (see below). Only rooms explicitly added to the frogLink configuration are available.

After configuring rooms:

```json
{"rooms":["Camper / Camper Inside","Camper / Camper Control"]}
```

## Step 5: Configure Types (Optional)

Types are broadcast-style messages that target all devices of a specific type within a room (e.g., "all lights in Kitchen"). Types must be configured in the frogLink and associated with rooms.

After configuring types:

```json
{"types":["Open door"]}
```

You can query which types are available in a specific room:

```json
{"cmd":"types","room":"Camper / Camper Inside"}
```

Response:

```json
{"types":["Open door"]}
```

## Step 6: Deploy Configuration

After making changes in the ProjectApp:

1. **Deploy/sync** the configuration to the frogblue network. This pushes the updated configuration to all devices, including the frogLink.
2. The frogLink will update its internal configuration. The `config` field in the project response shows the deployment timestamp.

## Verifying the Configuration

After provisioning, verify over serial:

```json
{"cmd":"project"}
```

A fully provisioned frogLink returns complete project information:

```json
{
  "project": "WIST Camper",
  "frogLinkName": " Camper USB",
  "frogLinkRoom": "Camper Control",
  "swVersion": "1.9.11.3",
  "config": "10.04.2026 15:54:09",
  "address": "A8:36:7A:00:4E:AC",
  "nadd": "084C",
  "netId": "80",
  "api": "2.0"
}
```

Then check each resource:

```json
{"cmd":"messages"}
{"cmd":"rooms"}
{"cmd":"types"}
```

If all return non-empty arrays matching your configuration, the frogLink is ready to use. See the [Command Reference](03-command-reference.md) for sending and receiving commands.
