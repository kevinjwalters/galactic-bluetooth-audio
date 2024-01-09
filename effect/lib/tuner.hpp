/**
 * Copyright (c) 2023 Kevin J. Walters
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <cstring>
#include <cstdint>
#include <array>
#include "limits.h"

#include "pico/stdlib.h"

#include "bitstream.hpp"

// This MUST BE a multiple of the buffer sizes from Bluetooth and
// DMA ADC
static constexpr unsigned int OLD_SAMPLE_COUNT_TUNER = 4096 + 2048;  // = 6144
// static constexpr unsigned int SAMPLE_COUNT_TUNER = 4096; // 54380 us
// 6144 @ 96k works for C1 (32.70, square wave synth), B0 does not work
// static constexpr unsigned int SAMPLE_COUNT_TUNER = 8192; // 216580 us 

// The SparkFun Sound Detector wired up to GP27 is strangely noisy
// 500 = +- 1.5%  1000 = +/- 3.1%  2000 = +/- 6.1%
static int16_t zc_noise_magn = 1000;


class Tuner {
  private:
    constexpr static uint32_t NOT_CALCULATED = UINT32_MAX - 1;

    // TODO type for samples somewhere? needed? not the same thing as raw adc sample
    std::vector<int16_t> sample_array;
    std::array<size_t, 5> period_results;
    size_t result_idx;
    size_t sample_idx;
    uint32_t sample_rate;
    float sample_rate_f;
    Bitstream<> bitstream;
    size_t step;
    float min_period;
    float max_period;

    float calculateFrequency(size_t latest_period);
    uint32_t getCorrelation(std::vector<uint32_t> &corr,
                            uint32_t &max_count, uint32_t &min_count, size_t *est_index,
                            size_t lag);

  public:
    constexpr static float MIN_FREQUENCY = 32.0f;  // just below C1
    constexpr static float MAX_FREQUENCY = 520.0f;  // just below C5
    constexpr static unsigned int SAMPLE_COUNT_TUNER = 4096 + 2048; // = 6144

    constexpr static float A4_REF_FREQUENCY = 440.0f;

    // Lower and upper thresholds which both need to be met
    // for correlation to be considered to be significant
    constexpr static size_t COUNT_CORR_THR_MIN = SAMPLE_COUNT_TUNER / 2 * 26 / 100;
    constexpr static size_t COUNT_CORR_THR_MAX = SAMPLE_COUNT_TUNER / 2 * 42 / 100;
    constexpr static float COUNT_CORR_SUB_THR_FRAC = 0.15f;

    float frequency;         // The last value based on period_results array
    float latest_frequency;  // The last value

    Tuner() : sample_array(SAMPLE_COUNT_TUNER, 0),
        period_results(),
        result_idx(0),
        sample_idx(0),
        sample_rate(0),
        sample_rate_f(0.0f),
        bitstream(SAMPLE_COUNT_TUNER),
        step(1),
        min_period(0.0f),
        max_period(0.0f),
        frequency(0.0f),
        latest_frequency(0.0f) {
        period_results = {};
    };

    bool add_samples(int16_t *buffer, size_t count, uint16_t channels);
    bool add_samples(uint16_t *buffer, size_t count, uint16_t channels);
    void process();
    void setSampleRate(uint32_t rate);
    const std::vector<int16_t>& getSamples(void) { return sample_array; };
    int32_t findCross(bool pos_edge = true, size_t sample_mass = 20'000);
    void setStep(size_t step_);
};

