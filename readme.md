# Pomodoro Timer for M5Stack Papers3

A Pomodoro timer application for the M5Stack Papers3 (ESP32-S3 e-paper device), built with PlatformIO and Arduino. Features a dual-ring animated display, intuitive touch controls, and smart power management.

## Demo
https://www.youtube.com/watch?v=52mUNnoogpQ

## Quick Start

1. Connect M5Stack Papers3 via USB
2. Build: `pio run`
3. Upload: `pio run --target upload`
4. Touch Play (▶) to start 25-minute timer

## Features

- **Dual-ring animation:** Outer ring (60 dots) shows seconds, inner ring shows minutes
- **Timer presets:** 25-minute (standard), 5-minute (break), 30-minute (extended)
- **Touch controls:** Play, Pause, Stop buttons plus preset selectors
- **Anti-ghosting:** Auto-refresh every 5 minutes, manual refresh button
- **Smart sleep:** Auto-sleep after 5 minutes of inactivity
- **Battery monitoring:** Real-time battery level (60-second intervals)
- **Audio feedback:** Button sounds and timer completion alerts
- **Screensaver support:** Custom image from SD card before sleep (optional)

## Hardware Requirements

- M5Stack Papers3 (ESP32-S3, e-paper display)
- SD Card (optional, for custom screensaver image)

## Controls

- **Play (▶):** Start timer or resume from pause
- **Pause (⏸):** Pause running timer
- **Stop (⏹):** Reset timer to selected duration
- **25Min/5Min/30Min:** Change timer duration
- **Refresh (🔄):** Clear e-paper ghosting manually

## Optional Setup

For custom screensaver:
1. Create `/pomodoro/` folder on SD card
2. Add `pomodoro.png` image (540x540 pixels)
3. Insert SD card into device

## Libraries

- [M5Unified](https://github.com/m5stack/M5Unified)
- [M5GFX](https://github.com/m5stack/m5gfx)
- [epdiy](https://github.com/vroland/epdiy)

## Technical Details

- **Architecture:** Arduino framework with non-blocking timers
- **Display:** Selective updates with anti-ghosting every 5 minutes
- **Power:** Smart sleep when timer not running and no activity
- **Touch:** Multi-button collision detection system
- **Memory:** Optimized for ESP32-S3 with minimal footprint