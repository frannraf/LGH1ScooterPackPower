# LGH1ScooterPackPower

Arduino/STM32 sketch to wake or sleep the LG H1 e-scooter battery pack sold by Jehu Garcia:
https://www.youtube.com/watch?v=E87EcLeqIX0

This version keeps the original behavior and refreshes the sketch layout for the STM32duino 2.x core. As of 2026-05-20, the latest checked STM32duino release is `2.12.0`.

## Hardware

- Board: STM32F103 "Blue Pill" or another STM32F1 board with bxCAN `CAN1`
- LED/status output: onboard Blue Pill user LED on silkscreen `PC13` / `C13`
- CAN bitrate: `100 kbps`
- Repeated command interval: `500 ms`

| Function | Blue Pill silkscreen | Connects to |
| --- | --- | --- |
| CAN RX | `PB8` / `B8` | CAN transceiver `RXD` |
| CAN TX | `PB9` / `B9` | CAN transceiver `TXD` |
| USB serial D- | `PA11` / `A11` | Onboard USB port |
| USB serial D+ | `PA12` / `A12` | Onboard USB port |
| Status LED | `PC13` / `C13` | Onboard user LED |

The old `PB5` / `B5` on/off switch input is not used by the current auto-wake sketch.

Use a proper 3.3 V CAN transceiver between the STM32 pins and CANH/CANL. Do not connect `PB8`/`PB9` directly to the battery pack CAN bus.

## Arduino Setup

1. Install Arduino IDE 2.x.
2. Add the STM32duino Boards Manager URL:
   `https://github.com/stm32duino/BoardManagerFiles/raw/main/package_stmicroelectronics_index.json`
3. Install `STM32 MCU based boards` from Boards Manager.
4. Select a Generic STM32F1 / STM32F103 board option matching your board.
5. Open `Jump_Start/Jump_Start.ino` and upload with ST-LINK or your normal Blue Pill upload method.

Use these board options when you want USB serial debug output:

- Board: `Generic STM32F1 series`
- Board part number: `BluePill F103C8`
- USB support: `CDC (generic 'Serial' supersede U(S)ART)`
- Upload method: `OpenOCD STLink (SWD)`

Open the serial monitor at `115200` baud after upload. Because USB serial uses `PA11`/`PA12`, CAN is remapped to `PB8`/`PB9`.

If you do not want to open Arduino IDE, flash the prebuilt binary in `firmware/` with STM32CubeProgrammer or OpenOCD. See `firmware/README.md`.

No external Arduino CAN library is required. The sketch uses the STM32F1 bxCAN peripheral registers directly.

The Arduino sketch has two tabs:

- `Jump_Start.ino`: readable project behavior, USB serial startup, LED states, and wake timing
- `CAN.ino`: CAN pin mapping, bitrate, bxCAN register setup, transmit, and serial receive printing

## Behavior

The sketch sends standard CAN ID `0x630` with this eight-byte payload:

```text
2F 00 22 01 XX 00 00 00
```

`XX` is:

- `01` for wake/on request
- `00` for sleep/off request, though the current auto-wake behavior only sends wake/on

On boot, the controller waits `1 second`, sends ten wake/on frames spaced `500 ms` apart, then keeps sending a wake/on frame every `500 ms`.

LED states:

- Short blink at boot: firmware started
- Repeating short blink during the 1-second startup delay: waiting before the wake attempt
- Three quick flashes plus a long pause: CAN initialization or wake transmit failed
- Breathing pattern: startup wake frames were transmitted successfully

## Common Tweaks

Change the constants near the top of each Arduino tab:

- `Jump_Start.ino`: `kBootWakeDelayMs`, `kWakeIntervalMs`, `kStartupWakeAttempts`, and LED timing constants
- `CAN.ino`: `kCanBitrate` for CAN speed and `kCanPins` for CAN remap selection

The included images show the original project wiring and CAN pinout.
