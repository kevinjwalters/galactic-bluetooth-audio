/**
 * Copyright (c) 2023 Pimoroni Ltd <phil@pimoroni.com>
 * Copyright (c) 2023 Kevin J. Walters
 *
 * SPDX-License-Identifier: MIT
 */

#include "effect/effect.hpp"

#include "effect/lib/fixed_fft.hpp"
#include "effect/lib/rgb.hpp"


void EffectFFT::update(int16_t *buffer, size_t sample_count, uint16_t channels) {
    if (!fft.add_samples(buffer, sample_count, channels)) {
        return;  // buffer not yet full
    }
    fft.process();
    updateDisplay();
}

// Note uint16_t * vs int16_t *
void EffectFFT::update(uint16_t *buffer, size_t sample_count, uint16_t channels) {
    if (!fft.add_samples(buffer, sample_count, channels)) {
        return;  // buffer not yet full
    }
    fft.process();
    updateDisplay();
}

// This is a static method
int EffectFFT::find_level(fix15 *levels, size_t count, fix15 value) {

    // TODO binary search
    size_t idx = 0;
    while (idx < count) {
        if (value > levels[idx]) {
            ++idx;
        } else if (value < levels[idx]) {
            return idx - 1; 
        } else {
            return idx;
        }
    }
    return idx - 1;
};

