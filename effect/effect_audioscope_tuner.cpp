/**
 * Copyright (c) 2023 Kevin J. Walters
 *
 * SPDX-License-Identifier: MIT
 */


#include <cmath>
#include <cstdint>
#include <string>

#include "pico_graphics.hpp"

#include "effect/effect.hpp"
#include "effect/lib/rgb.hpp"
#include "uad/debug.hpp"
#include "uad/display.hpp"


void EffectAudioscopeTuner::updateDisplay(void) {
    float frequency = tuner.latest_frequency;
    auto signal = tuner.getSamples();
 
    display.clear();    
    int32_t waveform_idx = 0;
    if (frequency != 0.0f) {
        // TODO consder a fixed 5 char of text at 5x5 + 4*5 position
        if (frequency < 99'999.49f) {
            freq_text.text(std::to_string(int32_t(roundf(tuner.frequency))),
                           5);
        }
        int32_t zc_idx = tuner.findCross();
        if (zc_idx >= 0) {
            waveform_idx = zc_idx;
        }
    }

    // TODO ensure tuner.sample_array access does not go beyond buffer
    // TODO add multiple fading passes
    for (uint8_t x=0; x < Display::WIDTH; x++) {
        int32_t y = Y_MID_POS - int32_t(roundf(signal[waveform_idx] * y_scale));
        waveform_idx += 10; // TODO proper (fast) reasmpling
        if (y >=0 && y < Display::HEIGHT) {
            display.set_pixel(x, y,
                              0u, 128u, 0u);
        }
    }             
}

void EffectAudioscopeTuner::start(void) {
    display.clear();    
}

void EffectAudioscopeTuner::init(void) {
    if (debug >=1) {
        printf("EffectAudioscopeTuner: %ix%i\n", display.WIDTH, display.HEIGHT);
    }
}

