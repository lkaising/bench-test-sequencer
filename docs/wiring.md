# Bench Test Wiring — Pin Assignments & Circuit

## Overview

This document describes the bench test wiring for validating the
Arduino → Mightex SLC-SA04-US → NIR LED signal chain using a momentary
pushbutton in place of the camera's ExposureActive signal.

## Pin Assignments

| Arduino Pin | Direction | Function                | Connected To              |
|-------------|-----------|-------------------------|---------------------------|
| D2          | INPUT     | Frame tick (INT0)       | Pushbutton via RC debounce|
| D8          | OUTPUT    | Channel 1 trigger       | SLC-SA04-US Ch1 TRIG IN   |
| D9          | OUTPUT    | Channel 2 trigger       | SLC-SA04-US Ch2 TRIG IN   |
| D10         | OUTPUT    | Channel 3 trigger       | SLC-SA04-US Ch3 TRIG IN   |
| GND         | —         | Logic ground            | SLC TRIG GND              |
| +5V         | —         | Debounce circuit power  | Pull-down / button rail   |

## Button + RC Debounce Circuit

```
                  +5V
                   │
              ┌────┴────┐
              │  Button │  (momentary, normally open)
              │   (NO)  │
              └────┬────┘
                   │
                   ├──────────── Arduino D2 (INT0)
                   │
               ┌───┴───┐
               │       │
              ┌┴┐     ┌┴┐
              │ │     │ │
              │ │     │ │
              └┬┘     └┬┘
               │       │
            10 kΩ   100 nF
          (pull-   (debounce
           down)      cap)
               │       │
               └───┬───┘
                   │
                  GND
```

**Behavior:**
- Button **not pressed**: D2 held LOW by 10 kΩ pull-down to GND
- Button **pressed**: D2 pulled HIGH through button to +5V
- Button **released**: D2 returns LOW via pull-down
- RC time constant: τ = 10 kΩ × 100 nF = **1 ms** (hardware debounce)
- Software debounce: **5 ms** additional guard in ISR

## Arduino → SLC Trigger Wiring

```
Arduino D8  ────────── SLC Ch1 TRIG IN   (850 nm / M850L3)
Arduino D9  ────────── SLC Ch2 TRIG IN   (940 nm / M940L3)
Arduino D10 ────────── SLC Ch3 TRIG IN   (1050 nm / M1050L4)
Arduino GND ────────── SLC TRIG GND      (shared logic ground)
```

**Electrical notes:**
- Arduino 5V GPIO meets SLC trigger threshold (4.5–10V HIGH)
- Direct wiring — no level shifter or buffer needed
- SLC trigger inputs are opto-isolated and high-impedance

## SLC → LED Wiring

Pre-existing - uses CON8ML-4 connectors with ferrule-terminated wires to SLC channel outputs.

**SLC configuration (pre-programmed via RS232):**
- All channels: TRIGGER follower mode (Tset = 9999)
- Ch1: 1200 mA (850 nm), Ch2: 1000 mA (940 nm), Ch3: 600 mA (1050 nm)
- Polarity: rising edge (pol = 0)

## Differences from Production Wiring

| Element              | Bench Test                  | Production                    |
|----------------------|-----------------------------|-------------------------------|
| D2 signal source     | Pushbutton + RC debounce    | Camera GPO2 (ExposureActive)  |
| D2 pull resistor     | 10 kΩ pull-down to GND      | 1 kΩ pull-up to +5V           |
| D2 idle state        | LOW (button not pressed)    | LOW (camera not exposing)     |
| D2 active state      | HIGH (button pressed)       | HIGH (camera exposing)        |
| Signal frequency     | ~1 Hz (manual)              | ~88 Hz (camera frame rate)    |
| Debounce             | RC + software               | Not needed (clean digital)    |

**Note:** D8/D9/D10/GND wiring to SLC is identical in both configurations.
When transitioning to the camera, only the D2 input circuit changes.

## BOM (Debounce Circuit Only)

| Item                          | Qty | Notes                           |
|-------------------------------|-----|---------------------------------|
| Momentary pushbutton (NO)     | 1   | Any tactile switch              |
| 10 kΩ resistor (¼W)           | 1   | Pull-down for D2                |
| 100 nF ceramic capacitor      | 1   | Debounce filter                 |
| Breadboard                    | 1   | For prototyping connections     |
| Jumper wires (M-M)            | 11  | Button circuit + D2 connection  |
