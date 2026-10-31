# Repository Guidelines

## Project Structure & Module Organization

Bepio is an ESP-IDF v5.5.3 firmware scaffold for ESP32-S3 DevKitC with an external
MN316 NB-IoT modem and an ES8311 + NS4150B audio module. Follow the component
structure of the local `D:/Codes/DeepStoa` reference. Business code lives in root
`apps/`; system capabilities live in `system/`. `main/Bepio.c` owns app_main,
initializes services and registers applications explicitly. `system/appmgr/` owns
application lifecycle, `system/controller/` hardware startup, `system/services/`
long-lived services, and `system/platform/` the ESP-IDF adapters. Business apps keep
app/model/view/controller layers. `boards/esp32s3/` owns pin definitions, `drivers/` hardware access, `modules/`
shared libraries, `assets/` resources, and `docs/` architecture/hardware decisions.
Preserve vendor material under `refer/YNH-MN316/`. `archive/zephyr/` is a historical
backup and must not become a current build dependency.

## Build, Test, and Development Commands

- `./build.ps1`: build with the local ESP-IDF installation.
- `./build.ps1 -Pristine`: clean and rebuild the application.
- `./build.ps1 -IdfPath C:/Espressif/frameworks/esp-idf-v5.5.3`: select ESP-IDF.
- `./build.ps1 -MessagesDemo`: separate build with explicit simulated messages.
- `idf.py build`: build from an activated ESP-IDF environment.
- `idf.py -p <PORT> flash monitor`: flash and monitor an explicitly selected board.

The helper defaults to `C:/Espressif/frameworks/esp-idf-v5.5.3`. Do not commit
build output, generated sdkconfig, or local credentials. Use ESP-IDF's standard
bootloader/partition/app flashing layout, not the former Zephyr image command.

## Coding Style & Naming Conventions

Use tab indentation in C, snake_case functions/files, uppercase macros, and guarded
headers. Prefix new project APIs with `bepio_`; keep private symbols static.
Prefer fixed buffers and bounded waits. Use native ESP-IDF/FreeRTOS APIs rather
than STM32 HAL, RT-Thread, or Zephyr compatibility APIs. Keep application logic
independent of direct UART/GPIO access. `.editorconfig` defines whitespace.

## Testing Guidelines

Existing GCC host regressions:

- `python tests/unittest/app_manager/run.py`
- `python tests/unittest/contacts/run.py`
- `python tests/unittest/messages/run.py`
- `python tests/unittest/console/run.py`

Run relevant regressions and ESP-IDF firmware builds for platform changes.
Host mutex mocks are single-threaded and do not prove FreeRTOS scheduling behavior.
Document hardware checks, including USB console, UART timeout and modem response.
Compilation does not prove cellular registration, messaging, or audio operation.

## Hardware Assumptions

Keep MN316 UART1 TX=17/RX=18 at 9600 8N1 and native USB GPIO19/20.
Keep audio SDA=41/SCL=42/MCLK=7/BCLK=4/LRCK=15/codec-DOUT=6/codec-DIN=16
unless the user authorizes rewiring. Current board header uses GPIO7 for MCLK;
GPIO48 may share the board RGB LED. Flash is 16 MB;
The user confirmed ESP32-S3 N16R8 (8 MB Octal PSRAM). Main firmware keeps PSRAM
disabled; debug/t_multinet enables it for MultiNet7. Hardware PSRAM and speech
recognition checks remain pending. Audio definitions do not initialize ES8311.

## Commit & Pull Request Guidelines

This directory has no Git history. Use concise imperative subjects and focused
changes. Explain behavior and build/hardware validation, include screenshots for UI
changes, and call out unverified hardware assumptions. Preserve EPOS/ZSWatch GPL
notices and the LICENSES directory.
