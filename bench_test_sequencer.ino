/*
 * File: bench_test_sequencer.ino
 * Purpose: Arduino entrypoint that forwards setup() and loop() to the sequencer implementation.
 * Project: Bench Test LED Sequencer (NIR Vein-Mapping System, Yarmush Lab, Rutgers BME)
 * Author: Logan Kaising
 * Copyright (c) 2026 Logan Kaising. All rights reserved.
 */

#include "src/bench_test_sequencer.h"

void setup() {
    sequencer_setup();
}

void loop() {
    sequencer_loop();
}
