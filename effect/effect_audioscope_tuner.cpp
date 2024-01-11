/**
 * Copyright (c) 2023 Kevin J. Walters
 *
 * SPDX-License-Identifier: MIT
 */


#include <array>
#include <cmath>
#include <cstdint>
#include <string>

#include "pico_graphics.hpp"

#include "effect/effect.hpp"
#include "effect/lib/rgb.hpp"
#include "uad/debug.hpp"
#include "uad/display.hpp"


static const std::array win5_32{3, 7, 12, 7, 3};  // TODO ponder symmetry
static constexpr uint32_t win5_32_shift = 5;

static inline int16_t window_win5_32(std::vector <int16_t> data1, int32_t idx) {
    int32_t value = 0;

    size_t half_win_len = std::size(win5_32) / 2;
    int32_t d_idx = idx - half_win_len;
    size_t data1_len = data1.size();
    if (d_idx >= 0 && d_idx < data1_len - std::size(win5_32)) {
        // More efficient version for most cases
        for (const auto weight : win5_32) {
            value += weight * int32_t(data1[d_idx++]);
        }
        value = value >> win5_32_shift;
    } else {
        int32_t weights = 0;
        for (const auto weight : win5_32) {
            if (d_idx >= 0 && d_idx < data1_len) {
                value += weight * data1[d_idx];
                weights += weight;
            };
            ++d_idx;
        }
        if (weights != 0) {
            value /= weights;
        }
    }

    return int16_t(value);
}

static inline int16_t window_win3_4(std::vector <int16_t> data1, int32_t idx) {

    if (idx >= 1 && idx < data1.size() - 1) {
        int32_t value = 0;
        // These are 1/4 2/4 1/4 weights
        value = data1[idx - 1 ] + (data1[idx] << 1) + data1[idx + 1];
        value = value >> 2;
        return int16_t(value);
    } else {
        return data1[idx];
    }
}

static inline int16_t window(std::vector <int16_t> data1, int32_t window, int32_t idx) {
    int32_t value = 0; 

    if (window == 5) {
        value = window_win5_32(data1, idx);
    } else if (window == 3) {
        value = window_win3_4(data1, idx);
    } else {
        value = data1[idx];        
    }

    return int16_t(value);
}

void EffectAudioscopeTuner::updateDisplay(void) {
    float frequency = tuner.latest_frequency;
    auto signal = tuner.getSamples();
 
    display.clear();    
    int32_t waveform_idx = 0;
    if (frequency != 0.0f) {
        int32_t zc_idx = tuner.findCross();
        if (zc_idx >= 0) {
            waveform_idx = zc_idx;
        }
    }

    auto &new_trace = traces[trace_idx];
    for (uint8_t x=0; x < Display::WIDTH; x++) {
        int32_t value = window(signal, 5, waveform_idx);  // returns 0 beyond end of buffer
        int32_t y = Y_MID_POS - int32_t(roundf(value * y_scale));
        new_trace[x] = (y >=0 && y < Display::HEIGHT) ? y : OFF_SCREEN;
        waveform_idx += 1000 / Display::WIDTH;  // TODO tune this based on sample rate
    }
    trace_idx = (trace_idx + 1) % traces.size();
    drawTraces();

    // Draw frequency counter value over scope traces
    // TODO consder a fixed 5 char of text at 5x5 + 4*5 position
    if (frequency > 0.0f && frequency < 99'999.49f) {
        freq_text.text(std::to_string(int32_t(roundf(tuner.frequency))),
                       5);
    }
}

void EffectAudioscopeTuner::drawTraces(void) {
    size_t t_idx = trace_idx;
    uint8_t intensity_idx = 0;
    for (size_t idx=0; idx < traces.size(); idx++) {
        const auto &trace = traces[t_idx++];
        for (uint8_t x=0; x < Display::WIDTH; x++) {
            int32_t y = trace[x];
            if (y >=0 && y < Display::HEIGHT) {
                display.set_pixel(x, y,
                                  0u, trace_intensity[idx], 0u);
            }
        }
        if (t_idx >= traces.size()) {
            t_idx = 0;  // wrap back to the start
        }
    }
}

void EffectAudioscopeTuner::start(void) {
    display.clear();    

    // default step is 1, other effects may have changed it
    tuner.setStep(4);

    // For visual effect, initialise it falling towards 0 scope pos
    int8_t start_row = Y_MID_POS - ((Y_MID_POS + 1) / 2);
    for (auto &trace : traces) {
        for (auto &tx : trace) {
            tx = start_row;
        }
    }
}

void EffectAudioscopeTuner::init(void) {
    if (debug >=1) {
        printf("EffectAudioscopeTuner: %ix%i\n", display.WIDTH, display.HEIGHT);
    }
}

