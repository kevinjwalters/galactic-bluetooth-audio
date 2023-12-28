/**
 * Copyright (c) 2023 Kevin J. Walters
 *
 * SPDX-License-Identifier: MIT
 */


#include "lib/fixed_fft.hpp"
#include "lib/rgb.hpp"
#include "effect.hpp"


void EffectTuner::update(int16_t *buffer, size_t sample_count, uint16_t channels) {
    if (!tuner.add_samples(buffer, sample_count, channels)) {
        return;  // buffer not yet full
    }
    tuner.process();
    updateDisplay();
}

// Note uint16_t * vs int16_t *
void EffectTuner::update(uint16_t *buffer, size_t sample_count, uint16_t channels) {
    if (!tuner.add_samples(buffer, sample_count, channels)) {
        return;  // buffer not yet full
    }
    tuner.process();
    updateDisplay();
}

