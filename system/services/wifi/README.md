# Wi-Fi station service

The Settings interaction follows NomadCast apps/settings (toggle, scan, saved networks, BSSID/channel and password entry), adapted to Bepio's native ESP-IDF service boundary. No source dependencies on NomadCast, board pin changes, or NomadCast HAL compatibility layers are introduced.

The radio is off at boot. Requests originate on the UI thread; initialization creates a worker, fixed-size single-slot command queue and synchronized snapshot. The worker owns driver setup, scan, association and storage. No event callback touches LVGL. Snapshots contain no passwords. The app polls snapshots every 250ms while a Wi-Fi page is visible. Leaving Settings removes the UI timer while the radio remains active.

Enabling starts a scan and tries the most recent saved SSID present in the results. Manual Scan only lists networks. Up to 16 scan records are read, empty SSIDs are skipped and repeated SSIDs use the strongest result supplied by IDF. Individual rows retain the scanned BSSID/channel. Open and WPA/WPA2/WPA3 personal networks are supported; enterprise, WEP and hidden-network manual entry are outside this UI.

Connection has a bounded 15-second wait for a DHCP address after driver setup; an intentional-disconnect drain is bounded to 500ms. Failed authentication or timeout does not save the attempted password. Disabling supersedes queued work and wakes a pending association wait; a blocking background scan can finish before radio stop. Operation generations discard stale results. DHCP loss/disconnect clears verified connectivity; address renewal refreshes it.

The four most recent successful SSID/password pairs use the bepio_wifi NVS namespace and atomic platform blob replacement. Wi-Fi enable state is session-only; enable Wi-Fi after reboot to reconnect with saved credentials. Storage failures are surfaced. Passwords are masked by default, revealed by the visibility icon button, and never logged. Known credentials are passed directly inside the service, not returned to the UI.

Wi-Fi association plus DHCP is not proof of Internet or backend availability. The service does not mark the account/message server online or replace the mock OTA flow. SSIDs with characters outside the current UI font may not render fully; connection uses the original bytes.

Validation: python tests/unittest/wifi/run.py builds the actual service with deterministic SDK mocks for request queuing, strongest-SSID deduplication, BSSID/channel, validation, DHCP success, password failure, timeout, cancellation, saved reconnect, IP renewal and scan/NVS errors. Mocks do not prove FreeRTOS scheduling or RF. Actual scan, password input, router association, DHCP and reboot reconnect require the board.

Wi-Fi UI: no manual Scan button. Turning the switch on automatically scans; toggle off/on to rescan. Results show one SSID line per 33px item.
