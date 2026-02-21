# bench_test_sequencer

**NIR Vein-Mapping System — Bench Test LED Sequencer**

Temporary firmware for validating the Arduino -> Mightex SLC-SA04-US -> NIR LED signal chain using a pushbutton in place of the camera trigger.

## What This Is

A bench-test variant of the production `led_sequencer` firmware. A momentary pushbutton with RC debounce substitutes for the camera's ExposureActive signal on D2. Each button press advances the channel sequence (850 nm -> 940 nm -> 1050 nm -> repeat) and the corresponding LED fires while the button is held down.

This validates the full TTL pipeline end-to-end without the camera.

## Quick Start

```bash
# 1. Install pinned AVR core (first time only)
make setup

# 2. Connect Arduino via USB, verify detection
make list

# 3. Build and flash
make upload

# 4. Open serial monitor to see channel reports
make monitor
```

On boot, the serial monitor (115200 baud) prints a startup banner and then reports each button press with the active channel and wavelength.

## Project Structure

```
bench_test_sequencer/
├── README.md                   # This file
├── Makefile                    # Build automation (setup/build/upload/monitor/clean)
├── bench_test_sequencer.ino    # Firmware source
├── .gitignore                  # Excludes build/ and config/local.mk
├── config/
│   ├── arduino-cli.yaml        # Pinned AVR core
│   ├── local.mk                # (gitignored) Per-machine overrides
│   └── local.mk.example        # Template for local.mk
└── docs/
    └── wiring.md               # Full pin assignments and circuit diagram
```

## Wiring Summary

**Button circuit (camera substitute):**
```
+5V → pushbutton (NO) → D2 → 10 kΩ → GND
                         └── 100 nF → GND
```

**Arduino → SLC triggers (identical to production):**
```
D8  → SLC Ch1 TRIG IN   (850 nm)
D9  → SLC Ch2 TRIG IN   (940 nm)
D10 → SLC Ch3 TRIG IN   (1050 nm)
GND → SLC TRIG GND
```

Full wiring details: [docs/wiring.md](docs/wiring.md)

## Test Procedure

### Prerequisites
- SLC-SA04-US powered and pre-programmed in TRIGGER follower mode (Tset = 9999, channels 1-3, via RS232)
- LEDs connected to SLC channel outputs via CON8ML-4 cables
- Arduino wired per docs/wiring.md
- IR safety precautions in place (NIR LEDs are invisible - use IR card or camera to verify emission)

### Step 1: Serial Verification (No SLC)

1. Flash firmware: `make upload`
2. Open monitor: `make monitor`
3. Press button - observe serial output:
   ```
   Press #1 -> 850_nm
   Press #2 -> 940_nm
   Press #3 -> 1050_nm
   Press #4 -> 850_nm
   ```
4. Verify sequence is correct and no presses are skipped or doubled.

### Step 2: Oscilloscope Validation

Probe D2, D8, D9, D10 and verify:
- [ ] D2 shows clean rising/falling edges (no bounce visible)
- [ ] Only one of D8/D9/D10 goes HIGH per press
- [ ] Active output is HIGH only while D2 is HIGH (exposure-gated)
- [ ] Sequence advances correctly: D8 -> D9 -> D10 -> D8 -> ...

### Step 3: Full Signal Chain (With SLC + LEDs)

1. Power SLC, verify channels in TRIGGER mode: `?MODE 1`, `?MODE 2`, `?MODE 3`
2. Press button - corresponding LED should light while held
3. Release - LED should turn off immediately
4. Verify:
   - [ ] Correct LED fires for each channel (850 -> 940 -> 1050)
   - [ ] No LED stays on between presses
   - [ ] No cross-talk (only one LED at a time)
   - [ ] LEDs visually cycle in correct order (use IR card for NIR)

## Build-Time Options

Override via `make build BUILD_EXTRA_FLAGS="-DDEBOUNCE_US=10000"` or by editing the `#define` values in the sketch:

| Define           | Default | Description                             |
|------------------|---------|-----------------------------------------|
| `DEBOUNCE_US`    | 5000    | Software debounce threshold (µs). 0=off |
| `SERIAL_BANNER`  | 1       | Print startup info + per-press reports  |

## Transitioning to Camera

When the Allied Vision Alvium G1-130 VSWIR arrives:

1. Switch firmware back to production `led_sequencer.ino` (v3e)
2. Replace button circuit with camera opto-coupler connection:
   - Remove: pushbutton, 10 kΩ pull-down, 100 nF cap
   - Add: 1 kΩ pull-up from camera GPO2 (TFM Pin 6) to Arduino +5V
3. D8/D9/D10/GND wiring to SLC stays unchanged

## Author

Logan Kaising - Yarmush Lab, Rutgers Biomedical Engineering NIH Biotechnology Training Fellow
