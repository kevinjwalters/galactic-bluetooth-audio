/**
 * Copyright (c) 2023 Kevin J. Walters
 *
 * SPDX-License-Identifier: MIT
 */


#include "lib/fixed_fft.hpp"
#include "lib/rgb.hpp"
#include "effect.hpp"


void EffectRecorder::update(int16_t *buffer, size_t sample_count, uint16_t channels) {
    if (!recording) {
        return;
    }
    if (!recorder.add_samples(buffer, sample_count, channels)) {
        return;  // buffer not yet full
    }
    recorder.process();
    updateDisplay();
    recording = false;
}

// Note uint16_t * vs int16_t *
void EffectRecorder::update(uint16_t *buffer, size_t sample_count, uint16_t channels) {
    if (!recording) {
        return;
    }
    if (!recorder.add_samples(buffer, sample_count, channels)) {
        return;  // buffer not yet full
    }
    recorder.process();
    updateDisplay();
    recording = false;
}

void EffectRecorder::buttonB(void) {
    recording = true;
}

