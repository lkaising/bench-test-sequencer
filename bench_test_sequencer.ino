/*
 * bench_test_sequencer.ino
 * NIR Vein-Mapping System — Bench Test LED Sequencer
 * Target: Arduino Uno R3 / ATmega328P (16 MHz, 8-bit AVR)
 *
 * PURPOSE:
 *   Temporary bench-test variant of the production led_sequencer firmware.
 *   Substitutes a momentary pushbutton (with RC debounce) for the camera's
 *   ExposureActive signal, allowing end-to-end validation of the
 *   Arduino → Mightex SLC-SA04-US → NIR LED signal chain before the
 *   camera arrives.
 *
 * WHAT THIS TESTS:
 *   - INT0 interrupt fires on button press/release (rising/falling edges)
 *   - Round-robin channel sequencing (850 → 940 → 1050 → 850 ...)
 *   - SLC trigger follower mode (LED on while button held, off on release)
 *   - Correct wiring: D8→Ch1, D9→Ch2, D10→Ch3, shared GND
 *
 * DIFFERENCES FROM PRODUCTION (led_sequencer.ino v3e):
 *   1. Software debounce guard — rejects edges within DEBOUNCE_US of the
 *      last accepted edge. Uses micros() from Arduino core (acceptable at
 *      manual press rates; would add ~4 µs overhead at camera frame rates).
 *   2. Pull resistor documentation updated for bench test circuit
 *      (10 kΩ pull-down vs production 1 kΩ pull-up).
 *   3. DEBUG_HEARTBEAT defaults to 1 (visual liveness useful on bench).
 *   4. Serial output on startup to confirm firmware identity and readiness.
 *
 * BENCH TEST CIRCUIT:
 *   +5V ── pushbutton (NO) ── D2 ── 10 kΩ ── GND
 *                              │
 *                           100 nF ── GND
 *
 *   Button press → D2 goes HIGH (rising edge, LED on)
 *   Button release → D2 goes LOW (falling edge, LED off)
 *   RC debounce: τ = 10 kΩ × 100 nF = 1 ms (hardware)
 *   Software guard: 5 ms (belt-and-suspenders)
 *
 * TRANSITIONING TO PRODUCTION:
 *   When the camera arrives, switch back to led_sequencer.ino (v3e).
 *   Changes needed: remove debounce, swap pull resistor, disable serial.
 *   Pin assignments are identical — no wiring changes on D8/D9/D10/GND.
 *
 * Signal chain:
 *   Button ──► Arduino D2 (INT0) ──► D8/D9/D10 ──► SLC TRIG IN ──► LEDs
 *
 * Pin assignments:
 *   D2  → PD2 / INT0    (input:  pushbutton via RC debounce)
 *   D8  → PB0           (output: SLC Ch1 trigger — 850 nm / M850L3)
 *   D9  → PB1           (output: SLC Ch2 trigger — 940 nm / M940L3)
 *   D10 → PB2           (output: SLC Ch3 trigger — 1050 nm / M1050L4)
 *   D13 → PB5           (output: heartbeat indicator, toggles every 256 edges)
 *   GND                  (shared logic ground with SLC-SA04-US)
 *
 * Port ownership:
 *   This sketch assumes exclusive control of PORTB bits PB0–PB2 (D8–D10)
 *   and PB5 (D13) when DEBUG_HEARTBEAT is enabled. Do not use SPI
 *   concurrently.
 *
 * Version history:
 *   v1-bench  2026-02  Bench test variant from led_sequencer v3e
 *                      (Logan Kaising)
 */

#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdint.h>

// ========================= Build-Time Configuration =========================

// Software debounce threshold (microseconds).
// Edges arriving within this window after the last accepted edge are
// rejected. Set to 0 to disable (e.g., when driven by a function
// generator for automated testing).
#ifndef DEBOUNCE_US
#define DEBOUNCE_US 5000UL  // 5 ms — invisible at manual press rates
#endif

// Debug heartbeat on D13 (PB5 / onboard LED):
//   1 = Toggle D13 every 256 accepted rising edges for visual liveness.
//   0 = Disabled.
// Defaults to ON for bench testing.
#ifndef DEBUG_HEARTBEAT
#define DEBUG_HEARTBEAT 1
#endif

// Serial startup banner:
//   1 = Print firmware identity and pin assignments on boot (115200 baud).
//   0 = No serial — saves ~1 KB flash and avoids timer0 ISR interaction.
#ifndef SERIAL_BANNER
#define SERIAL_BANNER 1
#endif

// ========================= Pin & Channel Definitions ========================

static constexpr uint8_t kNumChannels = 3;
static_assert(kNumChannels >= 1 && kNumChannels <= 4,
              "Supports 1-4 channels on PORTB PB0..PB3");

// Pin mapping:
//   D2  → PD2 / INT0    (input:  pushbutton frame tick)
//   D8  → PB0           (output: SLC Ch1 trigger — 850 nm)
//   D9  → PB1           (output: SLC Ch2 trigger — 940 nm)
//   D10 → PB2           (output: SLC Ch3 trigger — 1050 nm)
static constexpr uint8_t kFrameTickBit = PD2;

// Precomputed bitmasks for each channel, in firing order.
static constexpr uint8_t kChannelMasks[kNumChannels] = {
    _BV(PB0),  // Ch0: 850 nm (M850L3)
    _BV(PB1),  // Ch1: 940 nm (M940L3)
    _BV(PB2),  // Ch2: 1050 nm (M1050L4)
};

// Combined mask of all channel pins.
static constexpr uint8_t kChannelMask =
    _BV(PB0) | _BV(PB1) | _BV(PB2);

// Wavelength names for serial output (progmem to save RAM).
static const char* const kChannelNames[kNumChannels] = {
    "850 nm", "940 nm", "1050 nm"
};

// ========================= Runtime State ====================================

// Index of the channel to fire on the *next* rising edge.
// Initialised to 0 → first press fires channel 0 (D8 / 850 nm).
volatile uint8_t g_nextChannel = 0;

// Timestamp of the last accepted edge (microseconds).
// Used by the software debounce guard.
volatile unsigned long g_lastEdgeUs = 0;

// Count of accepted rising edges (for serial reporting).
volatile uint8_t g_risingCount = 0;

// ========================= Inline Helpers ===================================

static inline void clearChannelsFast() {
    PORTB &= static_cast<uint8_t>(~kChannelMask);
}

static inline void switchChannelFast(uint8_t idx) {
    PORTB = (PORTB & static_cast<uint8_t>(~kChannelMask))
            | kChannelMasks[idx];
}

static inline uint8_t nextChannelIdx(uint8_t ch) {
    ch++;
    if (ch >= kNumChannels) ch = 0;
    return ch;
}

// ========================= INT0 ISR =========================================
//
// Exposure-gated operation (adapted for pushbutton):
//   INT0 triggers on ANY logical change on PD2. We disambiguate
//   rising vs falling by sampling PIND inside the ISR.
//   - Rising edge  → debounce check, then fire current channel, advance.
//   - Falling edge → debounce check, then clear all channels.
//
// Software debounce:
//   micros() is safe to call from ISR on AVR — it reads timer0 registers
//   with a brief cli/sei internally, which is fine since we're already
//   in an ISR (interrupts are disabled). The ~4 µs overhead is irrelevant
//   at manual button press rates.

ISR(INT0_vect) {
    // --- Software debounce guard ---
#if DEBOUNCE_US > 0
    unsigned long now = micros();
    unsigned long elapsed = now - g_lastEdgeUs;  // handles rollover
    if (elapsed < DEBOUNCE_US) {
        return;  // bounce — ignore this edge
    }
    g_lastEdgeUs = now;
#endif

    // Snapshot volatile channel index.
    uint8_t ch = g_nextChannel;

    // Disambiguate edge direction.
    const bool buttonPressed = (PIND & _BV(kFrameTickBit)) != 0;

    if (buttonPressed) {
        // Rising edge — button pressed: fire the current channel.
        switchChannelFast(ch);
        g_nextChannel = nextChannelIdx(ch);
        g_risingCount++;

#if DEBUG_HEARTBEAT
        // Toggle D13 every 256 accepted rising edges.
        static uint8_t frameCount = 0;
        if (++frameCount == 0) {
            PINB = _BV(PB5);
        }
#endif
    } else {
        // Falling edge — button released: all channels off.
        clearChannelsFast();
    }
}

// ========================= Arduino Entry Points =============================

void setup() {
    cli();

    // --- Channel outputs (D8/D9/D10 → PB0/PB1/PB2) ---
    clearChannelsFast();
    DDRB |= kChannelMask;

#if DEBUG_HEARTBEAT
    PORTB &= static_cast<uint8_t>(~_BV(PB5));
    DDRB  |= _BV(PB5);
#endif

    // --- Frame tick input (D2 → PD2 / INT0) ---
    DDRD &= static_cast<uint8_t>(~_BV(kFrameTickBit));  // ensure input
    // Bench test uses external 10 kΩ pull-down to GND (button to +5V).
    // Do NOT enable internal pull-up — it would fight the pull-down.
    // (Production firmware uses external 1 kΩ pull-up for camera opto.)

    // --- INT0 configuration ---
    EIFR = _BV(INTF0);  // clear pending flag

    // ISC01:ISC00 = 01 → any logical change triggers INT0.
    EICRA = (EICRA & static_cast<uint8_t>(~(_BV(ISC01) | _BV(ISC00))))
            | _BV(ISC00);

    EIMSK |= _BV(INT0);

    sei();

    // --- Serial startup banner ---
#if SERIAL_BANNER
    Serial.begin(115200);
    Serial.println(F("\n==================================="));
    Serial.println(F("NIR Vein-Mapping System"));
    Serial.println(F("Bench Test LED Sequencer v1-bench"));
    Serial.println(F("==================================="));
    Serial.println(F("Pin assignments:"));
    Serial.println(F("  D2  (IN)  : Pushbutton (frame tick)"));
    Serial.println(F("  D8  (OUT) : SLC Ch1 — 850 nm"));
    Serial.println(F("  D9  (OUT) : SLC Ch2 — 940 nm"));
    Serial.println(F("  D10 (OUT) : SLC Ch3 — 1050 nm"));
    Serial.println(F("  D13 (OUT) : Heartbeat (every 256 presses)"));
    Serial.println(F("Debounce: 5 ms (software) + 1 ms (RC hardware)"));
    Serial.println(F("==================================="));
    Serial.println(F("Ready. Press button to cycle LEDs."));
    Serial.println(F("Channel sequence: 850 → 940 → 1050 → repeat"));
    Serial.println();
#endif
}

void loop() {
    // --- Optional: periodic status report over serial ---
    // Useful during bench testing to confirm which channel was last fired.
#if SERIAL_BANNER
    static uint8_t lastReported = 0;
    uint8_t current;

    // Read volatile with interrupts briefly disabled to get consistent snapshot.
    cli();
    current = g_risingCount;
    sei();

    if (current != lastReported) {
        lastReported = current;
        // g_nextChannel holds the *next* channel to fire, so the one
        // that just fired is (g_nextChannel - 1) mod 3.
        cli();
        uint8_t next = g_nextChannel;
        sei();
        uint8_t justFired = (next == 0) ? kNumChannels - 1 : next - 1;

        Serial.print(F("Press #"));
        Serial.print(current);
        Serial.print(F(" → Channel "));
        Serial.print(justFired);
        Serial.print(F(" ("));
        Serial.print(kChannelNames[justFired]);
        Serial.print(F(") — D"));
        Serial.println(8 + justFired);
    }
#endif
}
