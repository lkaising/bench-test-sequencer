/*
 * bench_test_sequencer.ino — Bench Test LED Sequencer (v1-bench)
 * NIR Vein-Mapping System — Yarmush Lab, Rutgers BME
 *
 * Pushbutton on D2 (INT0) substitutes for camera ExposureActive signal.
 * Each press advances the channel sequence (850→940→1050→repeat);
 * the corresponding SLC trigger is HIGH only while the button is held.
 *
 * Pins: D2 (input), D8/D9/D10 (SLC Ch1-3 triggers)
 * See docs/wiring.md for circuit details.
 */

#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdint.h>

// ── Hardware register aliases ───────────────────────────────────────────
#define LED_TRIGGER_PORT    PORTB
#define LED_TRIGGER_DDR     DDRB
#define TRIGGER_INPUT_PINS  PIND
#define TRIGGER_INPUT_DDR   DDRD

// ── Build-time options ──────────────────────────────────────────────────
#ifndef DEBOUNCE_US
#define DEBOUNCE_US 5000UL
#endif
#ifndef SERIAL_BANNER
#define SERIAL_BANNER 1
#endif

// ── Pin & channel definitions ───────────────────────────────────────────
static constexpr uint8_t kNumChannels     = 3;
static constexpr uint8_t kTriggerInputBit = PD2;
static constexpr uint8_t kChannelOutputBits[kNumChannels] = { _BV(PB0), _BV(PB1), _BV(PB2) };
static constexpr uint8_t kAllChannelBits  = _BV(PB0) | _BV(PB1) | _BV(PB2);
static const char* const kChannelNames[kNumChannels] = { "850_nm", "940_nm", "1050_nm" };

// ── Runtime state (shared with ISR) ─────────────────────────────────────
volatile uint8_t       g_nextChannelIndex   = 0;
volatile unsigned long g_lastAcceptedEdgeUs = 0;
volatile uint8_t       g_triggerCount       = 0;

// ── Helpers ─────────────────────────────────────────────────────────────
static inline void allChannelsOff() {
    LED_TRIGGER_PORT &= static_cast<uint8_t>(~kAllChannelBits);
}
static inline void setActiveChannel(uint8_t channel) {
    allChannelsOff();
    LED_TRIGGER_PORT |= kChannelOutputBits[channel];
}
static inline uint8_t advanceChannel(uint8_t channel) {
    channel++;
    return (channel >= kNumChannels) ? 0 : channel;
}

// ── INT0 ISR — fires on any logic change on D2 ─────────────────────────

ISR(INT0_vect) {
#if DEBOUNCE_US > 0
    unsigned long now = micros();
    if (now - g_lastAcceptedEdgeUs < DEBOUNCE_US) return;
    g_lastAcceptedEdgeUs = now;
#endif

    uint8_t ch = g_nextChannelIndex;
    bool pressed = (TRIGGER_INPUT_PINS & _BV(kTriggerInputBit)) != 0;

    if (pressed) {
        setActiveChannel(ch);
        g_nextChannelIndex = advanceChannel(ch);
        g_triggerCount++;
    } else {
        allChannelsOff();
    }
}

// ── setup ───────────────────────────────────────────────────────────────

void setup() {
    cli();

    allChannelsOff();
    LED_TRIGGER_DDR |= kAllChannelBits;

    TRIGGER_INPUT_DDR &= static_cast<uint8_t>(~_BV(kTriggerInputBit));

    EIFR  = _BV(INTF0);
    EICRA = (EICRA & static_cast<uint8_t>(~(_BV(ISC01) | _BV(ISC00))))
            | _BV(ISC00);
    EIMSK |= _BV(INT0);

    sei();

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
    Serial.println(F("Debounce: 5 ms (software) + 1 ms (RC hardware)"));
    Serial.println(F("==================================="));
    Serial.println(F("Ready. Press button to cycle LEDs."));
    Serial.println(F("Channel sequence: 850 → 940 → 1050 → repeat\n"));
#endif
}

// ── loop — serial press reporting ───────────────────────────────────────

void loop() {
#if SERIAL_BANNER
    static uint8_t lastReported = 0;

    cli();
    uint8_t current = g_triggerCount;
    sei();

    if (current != lastReported) {
        lastReported = current;

        cli();
        uint8_t next = g_nextChannelIndex;
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
