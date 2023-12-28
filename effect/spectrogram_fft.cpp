/**
 * Copyright (c) 2023 Kevin J. Walters
 *
 * SPDX-License-Identifier: MIT
 */

#include <cmath>

#include "effect/effect.hpp"

#include "uad/debug.hpp"
#include "effect/lib/fixed_fft.hpp"
#include "effect/lib/rgb.hpp"


void EffectSpectrogramFFT::updateDisplay() {
    // If this is the last row which will contribute to the mean value then
    // put that on the row and scroll it up but save some cycles and let
    // next update write new values to the row
    ++t_sample_count;
    bool last_update_write_mean = (t_sample_count == UPDATES_PER_SCROLL);
    for (uint8_t x = 0u; x < display.WIDTH; x++) {
        fix15 sample = fft.get_scaled_as_fix15(x + FFT_SKIP_BINS);

        sample_total[x] += sample;

        fix15 value = last_update_write_mean ?
            multiply_fix15_unit(mean_multiplier, sample_total[x]) : sample;
        int level = find_level(brightness_levels,
                               sizeof(brightness_levels) / sizeof(brightness_levels[0]),
                               value);
        if (level >= 0) {
            display.set_pixel(x, 0,
                              (uint16_t)palette[level].r,
                              (uint16_t)palette[level].g,
                              (uint16_t)palette[level].b);
        } else {
            display.set_pixel(x, 0, 0u, 0u, 0u);
        }
    }
    if (last_update_write_mean) {
        display.scroll('D');
        init_totals();
    }
}


void EffectSpectrogramFFT::start(void) {
    fft.set_scale(1000.0f);
}


void EffectSpectrogramFFT::init(void) {
    if (debug >=1) {
        printf("EffectSpectrogramFFT: %ix%i\n", display.WIDTH, display.HEIGHT);
    }

    init_totals();

    for(auto i = 0u; i < display.LEVELS; i++) {
        palette[i] = RGB::from_hsv(0.69f, 1.0f, 0.3f * (float)i / float(display.LEVELS - 1));
    }

    // Log mode - looks bad in a bad way not a bad way
    float db_range = 36.0f;
    float levels_to_bel = db_range / 20.0f / float(display.LEVELS);
    float low_bel = (0 - db_range) / 20.0f;
    for(auto i = 0u; i < display.LEVELS; i++) {
        brightness_levels[i] = float_to_fix15(1000.0f * powf(10.f, low_bel + float(i) * levels_to_bel));
    }

    // Linear mode
//    for(auto i = 0u; i < display.LEVELS; i++) {
//        brightness_levels[i] = float_to_fix15(1000.0f * float(i) /  float(display.LEVELS - 1));
//    }
}


void EffectSpectrogramFFT::init_totals(void) {
    t_sample_count = 0u;
    memset(sample_total, 0, sizeof(sample_total));
}

