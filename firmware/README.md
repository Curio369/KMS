# ESP32 enclosure firmware

Board: **ESP32 DevKit V1** (ESP32-WROOM-32). Pin map below is for that board.

## What it does

| Job | Detail |
|---|---|
| SoftAP `SNTC-Enclosure` | Serves a captive portal showing a 6-char proximity code |
| STA on campus WiFi | Uplink for MQTT — the AP itself has **no** internet route |
| Publishes the code | `device/{uuid}/access/proximity_code`, rotated every 60 s |
| Obeys commands | `unlock_door`, `dispense`, `unlock` — with nonce replay protection |
| Reports | door events, tamper, telemetry, heartbeat every 30 s |

The AP is deliberately a dead end. Members read the code, leave the AP, and
submit it from a real network. Full rationale in `docs/USER_GUIDE.md` §2.

## Cabinet ESP32 + Arduino Uno network handoff

For the cabinet gateway that drives the Arduino Uno over UART, use
`firmware/cabinet_esp`. It runs the ESP access point and upstream Wi-Fi at the
same time, but the access point does **not** provide internet routing to phones.

**The phone is not the transport.** One phone radio cannot be on
`ESP-KMS-xxxxxx` and on the internet at once, so the cabinet carries its own
uplink: the backend publishes dispense commands to the broker and the ESP32
picks them up over MQTT on its station interface. The phone joins the AP for
exactly one thing — reading the proximity code off the portal — then leaves and
uses mobile data for everything else.

```
phone (mobile data) → kms-opal.vercel.app → Render FastAPI
                                              ↓ MQTT/TLS 8883
                                           HiveMQ Cloud
                                              ↓
                          cabinet ESP32 (STA) → UART 9600 → Arduino Uno
```

Two independent gates, both still required: the authenticator/TOTP code proves
*who* you are and works from anywhere; the proximity code proves *where* you are
and only the cabinet knows it.

### Configure

Copy `firmware/cabinet_esp/secrets.h.example` to `secrets.h` (gitignored) and
fill in what differs from `config.h`:

```c
#define STA_SSID      "your-upstream-wifi"
#define STA_PASSWORD  "your-upstream-password"
#define MQTT_HOST     "xxxxx.s1.eu.hivemq.cloud"
#define MQTT_USERNAME "kms-cabinet"
#define MQTT_PASSWORD "..."
#define DEVICE_UUID   "..."      // only if you made your own device row
```

Everything in `config.h` is `#ifndef`-guarded, so an absent define keeps the
default. `MQTT_ROOT_CA` pins ISRG Root X1 (HiveMQ serves Let's Encrypt certs) —
not `setInsecure()`, because an unverified socket lets anyone forge `dispense`
and empty the rack. Re-check the chain before flashing a fleet:

```bash
openssl s_client -connect <host>:8883 -showcerts </dev/null
```

Trim `SLOT_ANGLE_OFFSET` once against the real rack. A belt, a coupler and a set
screw each add a few degrees the model cannot see.

### Campus captive portal

The IIT Mandi network associates with no 802.1X and then intercepts every
request until its login form is POSTed, so *associated* is not *online*. The
firmware probes `connectivitycheck.gstatic.com/generate_204` and POSTs the login
form when the probe comes back as anything other than a bare 204.

Leave `PORTAL_LOGIN` at `0` for a network that just works once it associates —
home Wi-Fi, a phone hotspot, a 4G MiFi.

**Capture the real POST, don't guess it.** Log in from a laptop with devtools →
Network open, find the login POST, and copy its URL and request body into
`secrets.h`, replacing the username and password values with `%s` in that order:

```c
#define PORTAL_LOGIN      1
#define PORTAL_LOGIN_URL  "http://10.0.0.1:8090/login.xml"
#define PORTAL_LOGIN_BODY "mode=191&username=%s&password=%s&a=0&producttype=0"
#define PORTAL_USERNAME   "your-ldap-id"
#define PORTAL_PASSWORD   "your-ldap-password"
```

Both values are percent-encoded before substitution, so a password containing
`&`, `+` or `%` is safe. Prove that path on the host without hardware:

```bash
cc -Wall -Wextra -o /tmp/tu firmware/tools/test_urlencode.c && /tmp/tu
```

Serial output on a portal network, at 115200:

```
[portal] traffic intercepted, logging in
[portal] login POST -> 200
[portal] session open
[mqtt] connected
```

A portal that needs a CSRF token or a cookie from a prior GET will **not** work
with this — the token is per-session and has to be scraped first.

Two things worth knowing before you flash a personal credential:

- Flash reads out over USB with `esptool.py read_flash`. No soldering, no
  exploit. Whoever opens the cabinet gets a working campus account, and if that
  account is SSO they get mail with it. Ask IT for a service account or an IoT
  MAC exemption first; failing that, rotate the credential when the project
  ends.
- If `PORTAL_LOGIN_URL` is `https`, `HTTPClient`'s single-argument `begin()`
  calls `setInsecure()` — the certificate is **not** verified, so anyone on the
  campus LAN can MITM the handshake and read the credential. Fixing that needs
  the portal's own CA pinned in `config.h`, and self-signed portals can't be
  pinned usefully at all.

The login only runs while MQTT is down. A live MQTT session is itself proof the
portal session is open, and its keepalives are what hold it open — so in steady
state this costs nothing.

### Render side

`render.yaml` sets `MQTT_ENABLED=true`, `MQTT_PORT=8883`, `MQTT_TLS=true`, and
prompts for `MQTT_HOST` / `MQTT_USERNAME` / `MQTT_PASSWORD` on first deploy. Use
a **separate** broker credential from the cabinet's so either can be revoked
alone.

### Arduino Uno side

`DEBUG_VIA_USB` must be `false`. With it `true` the sketch reads USB Serial and
never `commSerial`, so every command the ESP32 sends is silently discarded — the
dispense path is dead even over the local AP.

### Test it in layers

```bash
# 1. protocol parser, on the host — no hardware
cc -o /tmp/tp firmware/tools/test_protocol.c && /tmp/tp

# 2. Uno alone, over USB with DEBUG_VIA_USB temporarily true
#    send ANGLE:90 -> expect ACK: then DONE:

# 3. ESP alone: watch it publish
mosquitto_sub -h <host> -p 8883 --capath /etc/ssl/certs \
              -u <user> -P <pass> -t 'device/#' -v

# 4. command path, without the website
mosquitto_pub -h <host> -p 8883 --capath /etc/ssl/certs \
              -u <user> -P <pass> \
              -t 'device/<uuid>/rack/command' \
              -m '{"action":"dispense","slot_number":3,"nonce":12345}'
```

Then the full path with the phone on mobile data for everything except the code
read.

## Flash it

1. **Arduino IDE** → Preferences → Additional Board URLs:
   ```
   https://espressif.github.io/arduino-esp32/package_esp32_index.json
   ```
   Then Boards Manager → install **esp32** by Espressif.

2. Library Manager → install:
   - `PubSubClient` (Nick O'Leary)
   - `ArduinoJson` (Benoit Blanchon, v7+)

3. Edit `config.h`:
   ```c
   #define STA_SSID      "your-campus-ssid"
   #define STA_PASSWORD  "your-campus-password"
   #define MQTT_HOST     "192.168.1.50"   // machine running docker-compose
   ```
   `DEVICE_UUID` already matches the seeded demo device — leave it alone unless
   you created your own device in Admin → Devices.

4. Board: **DOIT ESP32 DEVKIT V1**. Select the port, Upload.

Or skip the IDE entirely — `arduino-cli` is scriptable:

```bash
arduino-cli compile --fqbn esp32:esp32:esp32 kms_enclosure
arduino-cli upload  --fqbn esp32:esp32:esp32 -p /dev/ttyUSB0 kms_enclosure
arduino-cli monitor -p /dev/ttyUSB0 -c baudrate=115200
```

Serial access needs your user in `dialout`: `sudo usermod -aG dialout $USER`,
then log out and back in.

5. Serial Monitor at **115200**. Expected:
   ```
   [ap] SNTC-Enclosure at 4.3.2.1
   [sta] up: 192.168.1.77
   [code] initial 4F2A91
   [mqtt] connected
   [code] 4F2A91 -> device/1111.../access/proximity_code
   ```

## Wiring

**Never use GPIO 6–11 on a WROOM-32 module** — they are wired to the internal
SPI flash, and driving them crashes the chip into a boot loop.

| Signal | GPIO | Notes |
|---|---|---|
| Door lock driver | 4 | HIGH = unlocked. Use a MOSFET/relay, not the pin directly |
| Tamper switch | 5 | NC to GND; opens (reads HIGH) when the enclosure is prised |
| Slots 1–8 | 13,14,16,17,18,19,21,22 | `PIN_SLOTS[]` in the `.ino`, HIGH for 1.2 s to actuate |

Change the `PIN_*` constants at the top of the `.ino` if your board differs.

## Testing without hardware

The whole proximity path can be exercised from a laptop — publish a code by
hand and it lands in Redis exactly as the ESP32's would:

```bash
docker compose up -d mosquitto redis postgres

mosquitto_pub -h localhost -t 'device/11111111-2222-3333-4444-555555555555/access/proximity_code' \
              -m '{"code":"TEST42"}'
```

Then enter `TEST42` at `/connect` in the portal. Watch commands go the other
way with:

```bash
mosquitto_sub -h localhost -t 'device/#' -v
```

## MQTT broker choice

- **Local dev** — `docker compose up mosquitto`, set `MQTT_HOST` to your
  machine's LAN IP (not `localhost`; the ESP32 resolves that to itself).
- **Production** — HiveMQ Cloud. Set `MQTT_USERNAME`/`MQTT_PASSWORD`, and note
  that TLS on port 8883 needs `WiFiClientSecure` instead of `WiFiClient`.

## Known limits

- `openDoor()` blocks for the unlock TTL. Fine for one door; add a timer if you
  ever actuate concurrently.
- Battery telemetry is hardcoded to 100%. Wire a fuel gauge and replace it.
- No OTA. Re-flash over USB.
