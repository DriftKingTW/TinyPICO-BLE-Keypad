# Schnell BLE Keypad

⌨️ Firmware for **Schnell Keypad** — a wireless macro keypad built on the
Espressif **ESP32-S3-WROOM-1**, by [DriftKingTW](https://github.com/DriftKingTW).

It works as both a **USB HID** keyboard and a **Bluetooth LE** keyboard, with
on-device layout switching, macros, a rotary encoder, an OLED display, and a
Wi-Fi configuration page.

## Features

- **Dual output** — switch on the fly between USB HID and BLE HID.
- **5 × 7 key matrix** with multiple, switchable layouts.
- **Macros** — key-stroke combos, string output, or string + Enter.
- **Tap-Toggle layers** — momentary layer switch, double-tap to lock.
- **Rotary encoders** — onboard encoder plus an optional I²C extension board
  (PCF8574: 3 keys + encoder).
- **Bi-directional switch** for quick layout cycling.
- **OLED status display** (SSD1306 128×32) — battery, connection, layout, key
  feedback.
- **WS2812 status LED** and **battery monitoring** with low-battery alert.
- **Power management** — auto screen sleep, deep sleep with key-press wake,
  "caffeinated" (stay-awake) and output-lock modes.
- **Configuration** — Wi-Fi web UI, [Improv](https://www.improv-wifi.com/)
  serial provisioning, or direct serial `keyconfig.json` upload.

## Hardware

| Part | Detail |
| --- | --- |
| MCU | ESP32-S3-WROOM-1 (N4R2 — 4 MB flash, 2 MB PSRAM) |
| Display | SSD1306 128×32 OLED (I²C) |
| LED | 1× WS2812 |
| Input | 5×7 matrix, onboard rotary encoder, bi-directional switch, 3 config buttons |
| Extension (optional) | PCF8574 rotary-encoder board @ `0x38` |

> The board definition lives in [`boards/esp32-s3-wroom-1-n4r2.json`](boards/esp32-s3-wroom-1-n4r2.json)
> so the project builds without any local PlatformIO customization.

## Project structure

The firmware is organized into focused modules under `src/`:

| Module | Responsibility |
| --- | --- |
| `main.cpp` | `setup()`/`loop()`, FreeRTOS tasks, key/power/config logic |
| `usbhid` / `blehid` | USB / BLE HID transport wrappers (each isolates one HID library) |
| `keyboard_output` | `KeyboardOutput` interface over USB/BLE |
| `config_store` | parses & caches `keyconfig.json` once |
| `display_state` | mutex-guarded OLED state |
| `web_server` | HTTP configuration server + Improv provisioning |
| `helper.hpp` | small SPIFFS/format helpers |

> ⚠️ **Note for contributors:** `USBHIDKeyboard.h` (TinyUSB) and `BleKeyboard.h`
> (NimBLE) define conflicting macros and **must not** be included in the same
> translation unit — that is why each is wrapped in its own module. Module `.cpp`
> files should not include `main.hpp` or `helper.hpp`; depend on `main` via
> `extern` declarations instead.

## Building

This is a [PlatformIO](https://platformio.org/) project.

```bash
pio run                 # build firmware
pio run -t upload       # build + flash over USB
pio run -t uploadfs     # build + flash the SPIFFS filesystem (web UI + config)
pio device monitor      # serial monitor
```

Environment: `esp32-s3-wroom-1-n4r2` (see [`platformio.ini`](platformio.ini)).

## Flashing a release

Each [release](https://github.com/DriftKingTW/Schnell-BLE-Keypad/releases) ships
prebuilt binaries. Flash offsets for the N4R2 (no-OTA) layout:

| File | Offset | Contents |
| --- | --- | --- |
| `firmware-merged.bin` | `0x0` | bootloader + partitions + app (program only) |
| `spiffs.bin` | `0x210000` | filesystem (web UI + default config) |

```bash
# Update the program only (keeps existing settings):
esptool.py --chip esp32s3 write_flash 0x0 firmware-merged.bin

# Fresh device (program + filesystem):
esptool.py --chip esp32s3 write_flash 0x0 firmware-merged.bin 0x210000 spiffs.bin
```

> Re-flashing `spiffs.bin` overwrites the on-device configuration with the
> packaged defaults. To change only the program, flash `firmware-merged.bin`
> alone. Helper scripts are also provided in [`tools/`](tools/).

## Configuration

- **Web UI** — long-press the boot-mode config button to enter Wi-Fi mode, then
  open `http://schnell.local` (or the IP shown on the OLED). Keymaps, macros,
  layouts and network settings can be edited live. The companion editor is the
  [Schnell Keypad Configuration Tool](https://github.com/DriftKingTW/Schnell-Keypad-Configuration-Tool).
- **Improv** — provision Wi-Fi credentials over serial.
- **Serial** — paste a `keyconfig.json` into the serial monitor to update the
  keymap directly.

## Releases & CI

GitHub Actions handles builds and releases automatically:

- **CI** ([`.github/workflows/ci.yml`](.github/workflows/ci.yml)) — compiles on
  every push / PR to `master`.
- **Release** ([`.github/workflows/release.yml`](.github/workflows/release.yml))
  — pushing a `v*` tag builds the firmware, SPIFFS image and merged binary, then
  publishes a GitHub Release with the artifacts attached.

The in-firmware version string is injected from the git tag at build time
(`scripts/version.py`), so it always matches the release.

```bash
git tag -a v1.2.0 -m "Release v1.2.0"   # use -beta.N / -rc.N for pre-releases
git push origin v1.2.0
```

## License

Licensed under the GNU General Public License — see [`LICENCE`](LICENCE).
