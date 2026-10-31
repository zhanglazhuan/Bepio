# Account and local usage

Settings displays the factory account ID, registration status, an editable ID,
server result, cooldown, TX/RX JSON bytes and cumulative connection minutes/seconds.
Custom IDs use 4¨C32 lowercase ASCII letters, digits or underscore. The reserved
`bepio-` namespace uses the full factory eFuse MAC (not a truncated/random ID).
Factory MAC uniqueness assumes genuine, unmodified hardware. Global account
reservation and ownership MUST be confirmed by the backend before deployment.

## Backend integration still required

There is no account server or JSON transport in this repository. No fabricated
success response or estimated payload length is used. An authenticated transport
must call `bepio_account_set_online`, poll `bepio_account_take(id, &attempt)`,
and complete the exact ID/attempt with `bepio_account_complete`. The UI never
blocks on network I/O. Requests expire after 30 seconds; stale attempts are rejected.
Register the factory ID with the server on provisioning; its initial status remains
pending until that integration is implemented. The factory ID itself is immutable
in hardware even after the display ID changes.

The server must bind IDs to authenticated device ownership, atomically reserve a
new ID and release the previous alias, enforce the 24-hour rule across reboots and
clients, return trusted UTC and the next permitted change time, and support
idempotent reconciliation after lost ACKs or power loss. Never treat a mere
availability check as a committed rename. A successful ACK starts the local
24-hour cooldown; failed requests do not consume it. After reboot without trusted
UTC the server enforces the persisted deadline; the UI conservatively shows 24h.
An NVS failure after a successful server commit is shown and needs reconciliation.

## Usage accounting

`bepio_messaging_set_online` forwards real connectivity changes. An independent
transport may call the account connectivity API directly. Call
`bepio_account_record(tx_bytes, rx_bytes)` at the actual JSON transport boundary:
count encoded UTF-8 bytes including escaping, framing JSON, control messages,
retransmissions, rejected messages and duplicates. Do not count draft text,
struct sizes or simulated demo messages. IP/TLS/MQTT/UART overhead and carrier
billing are outside this metric. There is currently no JSON transport to wire.

Statistics and confirmed account state use the `bepio_account` NVS namespace,
independent of the app lifecycle and preference defaults. Counters checkpoint
once per minute and on disconnect; sudden power loss can lose up to one minute.
Account changes commit immediately. NVS initialization errors never erase data.
The periodic timer checks timeout every five seconds and avoids per-packet writes.

## Validation

Firmware build and account/messages/console regressions pass locally.
Run `python tests/unittest/account/run.py` for deterministic identity, conflicts,
cooldown, stale response, byte/time accounting and NVS persistence checks.
These SDK mocks do not prove FreeRTOS concurrency or physical flash behavior.
UI screenshots and touch verification require a connected display or preview. Hardware verification
remains pending: settings touch/keyboard and scrolling, USB console, UART timeout,
MN316 responses, real connectivity accounting, NVS reboot/power interruption,
backend ID conflicts, rate limiting, stale ACK and lost ACK reconciliation.


## Settings pages (320x240)

The home screen has a 44px two-line account card without an ID heading, followed by one 33px touch row each for Datetime, Updates, Battery and About. The list scrolls vertically to reach the bottom entries. Each page has matching pages/settings_*.c/.h; view.c manages dispatch, refresh timers and cleanup. Back returns from a child page to Settings before exiting the app.

- ID retains account ID change, server confirmation and usage details. The home card shows the actual account snapshot; host screenshots use an explicit fixture.
- Datetime selects named time zones (including DST rules) and 24h/12h format. The shared UI runtime updates the status bar. Until a trusted clock is provided, the device displays --:-- rather than inventing a time. This change does not add NTP or cellular time synchronization.
- Battery enables display power saving: backlight off after 30 seconds of inactivity; the first touch wakes without activating a control. The modem, audio and CPU are not suspended. Physical backlight/wake verification remains pending.
- Updates uses reserved https://updates.example.com/bepio/manifest.json as a mock endpoint. Check and update progress are simulated and labeled MOCK. There is no network download, firmware write or restart. Real OTA still requires transport, an authenticated release endpoint and OTA partitions; the current factory app partition is 3 MiB; OTA partitions are still absent.
- About reads firmware/version/build, ESP-IDF, chip revision/cores, flash, heap and factory MAC through the platform adapter.

Datetime and power preferences remain RAM-only, survive app close/reopen, and reset on reboot. Account persistence remains unchanged.

Validation: Settings/controller host regression, actual LVGL settings_preview, and actual UI preference runtime preferences_preview cover page routing, card/list dimensions, time zone/format, 30-second blanking, wake-touch suppression, failed backlight operations and mock OTA timer cancellation. Firmware compilation does not prove real OTA or battery savings.

![Settings](../../docs/assets/settings-home-320x240.png)
![Datetime](../../docs/assets/settings-datetime-320x240.png)
![Battery](../../docs/assets/settings-battery-320x240.png)
![Updates mock](../../docs/assets/settings-updates-ready-320x240.png)


## Wi-Fi

A 33px Wi-Fi list entry appears immediately above Datetime. settings_wifi.c/.h implements enable/scan/results and settings_wifi_connect.c/.h implements masked password, Show, Connect and status. Back returns from connection to Wi-Fi, then to Settings. Radio and credentials live in system/services/wifi, not in the app or the LVGL callbacks.

The radio starts off; enabling scans and reconnects to a saved visible network when available. Successful credentials are saved in NVS, failed attempts are not. Scan/connection run in the worker so the UI remains responsive. The Wi-Fi switch state itself resets on reboot. Existing Usage and other Settings entries are preserved.

![Wi-Fi](../../docs/assets/settings-wifi-320x240.png)
![Wi-Fi password](../../docs/assets/settings-wifi-password-320x240.png)

The GUI preview uses named test networks and a documentation-only address. It is not an RF/hardware validation result.


## Flash Storage

Storage appears after Battery and before Usage, with the existing 33px row size. settings_storage.c/.h renders an internal-flash snapshot through platform/bepio_storage_info.c. It shows physical flash capacity, actual running image length versus its reserved app partition, file-system used/total/available bytes and NVS used/total entries. Firmware and files have separate usage bars because their partitions are not interchangeable.

The storage partition is SPIFFS, 0x9f0000 bytes (9.94 MiB), after the 3 MiB app and 3 MiB Chinese voice partitions. Messages mounts it for text snapshots and initializes it on first use. Storage offers 3/7/14 day chat retention, saved in NVS; deletion requires a trustworthy clock. Reading this screen does not format it.

Values are refreshed on page entry. Tests cover mounted/unmounted file systems, read failures, invalid usage counters, page routing and image/partition accounting. Preview screenshots use fixtures; real device values require board verification.

![Storage](../../docs/assets/settings-storage-320x240.png)
![Mounted filesystem preview](../../docs/assets/settings-storage-mounted-320x240.png)

Wi-Fi UI: no manual Scan button. Turning the switch on automatically scans; toggle off/on to rescan. Results show one SSID line per 33px item.
