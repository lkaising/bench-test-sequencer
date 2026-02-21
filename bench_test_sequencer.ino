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
volatile uint8_t       g_nextChannelIndex          = 0;
volatile unsigned long g_lastAcceptedTriggerEdgeUs = 0;
volatile uint8_t       g_triggerCount              = 0;

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
static inline uint8_t previousChannel(uint8_t channel) {
    return (channel == 0) ? kNumChannels - 1 : channel - 1;
}
static inline void configurePins() {
    LED_TRIGGER_DDR |= kAllChannelBits;
    TRIGGER_INPUT_DDR &= static_cast<uint8_t>(~_BV(kTriggerInputBit));
}
static inline void configureButtonInterrupt() {
    EIFR  = _BV(INTF0);
    EICRA = _BV(ISC00);
    EIMSK |= _BV(INT0);
}

// ── INT0 ISR — fires on any logic change on D2 ─────────────────────────
ISR(INT0_vect) {
#if DEBOUNCE_US > 0
    unsigned long nowUs = micros();
    if (nowUs - g_lastAcceptedTriggerEdgeUs < DEBOUNCE_US) return;
    g_lastAcceptedTriggerEdgeUs = nowUs;
#endif

    uint8_t channel = g_nextChannelIndex;
    bool triggerIsHigh = (TRIGGER_INPUT_PINS & _BV(kTriggerInputBit)) != 0;

    if (triggerIsHigh) {
        setActiveChannel(channel);
        g_nextChannelIndex = advanceChannel(channel);
        g_triggerCount++;
    } else {
        allChannelsOff();
    }
}

// ── setup ───────────────────────────────────────────────────────────────
void setup() {
    cli();
    allChannelsOff();
    configurePins()
    configureButtonInterrupt()
    sei();

#if SERIAL_BANNER
    Serial.begin(115200);
    Serial.println(F("\nBench Test LED Sequencer v1-bench"));
    Serial.println(F("850 -> 940 -> 1050 -> repeat"));
    Serial.println(F("Ready.\n"));
#endif
}

// ── loop — serial press reporting ───────────────────────────────────────
void loop() {
#if SERIAL_BANNER
    static uint8_t lastReportedTriggerCount = 0;

    cli();
    uint8_t triggerCountSnapshot = g_triggerCount;
    uint8_t nextChannelIndexSnapshot = g_nextChannelIndex;
    sei();

    if (triggerCountSnapshot != lastReportedTriggerCount) {
        lastReportedTriggerCount = triggerCountSnapshot;
        uint8_t lastFiredChannelIndex = previousChannel(nextChannelIndexSnapshot);

        Serial.print(F("Press #"));
        Serial.print(triggerCountSnapshot);
        Serial.print(F(" -> "));
        Serial.println(kChannelNames[lastFiredChannelIndex]);
    }
#endif
}
