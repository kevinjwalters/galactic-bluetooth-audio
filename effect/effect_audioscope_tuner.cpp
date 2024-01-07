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
        if (frequency < 99999.49f) {
            freq_text.text(std::to_string(int32_t(roundf(tuner.frequency))),
                           5);
        }
        int32_t zc_idx = findCross(signal);
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

// Move this to Tuner??
int32_t EffectAudioscopeTuner::findCross(const std::vector<int16_t> &signal, bool pos_edge, size_t sample_mass) {
    uint32_t seq_neg_mass = 0, seq_pos_mass = 0;
    size_t seq_neg_count = 0, seq_pos_count = 0;
    size_t matching_edge = 0;
    bool found_pre_cross = false;

    auto prev_elem = signal[0];
    int32_t idx = -1;
    for (idx = 0 ; idx < signal.size(); idx++) {
        auto elem = signal[idx];
        // count sequential positive and negative values
        if (elem > 0) {
            seq_pos_mass = (prev_elem > 0) ? seq_pos_mass + elem : 0;
            seq_pos_count = (prev_elem > 0) ? seq_pos_count + 1 : 0;
        } else if (elem < 0) {
            seq_neg_mass = (prev_elem < 0) ? seq_neg_mass - elem : 0;
            seq_neg_count = (prev_elem < 0) ? seq_neg_count + 1 : 0;
        }

        // we have found it if there's a sequential sequence of one sign
        // followed by the other sign possibly with some indecision in between
        if (pos_edge) {
            found_pre_cross = found_pre_cross || (seq_neg_mass >= sample_mass);
            if (found_pre_cross && seq_pos_mass >= sample_mass) {
                matching_edge = seq_pos_count;
                break;
            }
        } else {
            found_pre_cross = found_pre_cross || (seq_pos_mass >= sample_mass);
            if (found_pre_cross && seq_neg_mass >= sample_mass) {
                matching_edge = seq_neg_count;
                break;
            }
        }
        prev_elem = elem;
    }

    // -1 if nothing was found or backtrack to start of zero cross if possible
    return (matching_edge > 0) ? (idx >= matching_edge ? idx - matching_edge: 0) : -1;
}

