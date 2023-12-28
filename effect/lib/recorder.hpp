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
#include <array>

#include "pico/stdlib.h"

// static constexpr unsigned int SAMPLE_COUNT_RECORDER = 10'000u;  // works
// static constexpr unsigned int SAMPLE_COUNT_RECORDER = 70'000u;  // works
static constexpr unsigned int SAMPLE_COUNT_RECORDER = 80'000u;  // works some of the time!
// static constexpr unsigned int SAMPLE_COUNT_RECORDER = 90'000u;  // hangs
// static constexpr unsigned int SAMPLE_COUNT_RECORDER = 100'000u;  // hangs


class Recorder {
  private:
    float sample_rate;

    std::array<int16_t,SAMPLE_COUNT_RECORDER> sample_array;
    size_t sample_idx;

  public:

    Recorder() : Recorder(44100.0f) {};
    Recorder(float sample_rate) : sample_rate(sample_rate), sample_idx(0) {};

    bool add_samples(int16_t *buffer, size_t count, uint16_t channels);
    bool add_samples(uint16_t *buffer, size_t count, uint16_t channels);
    void process();
};
