/**
 * Copyright (c) 2023 Kevin J. Walters
 *
 * SPDX-License-Identifier: MIT
 */


#include "effect/lib/recorder.hpp"
#include <inttypes.h>
#include <cstdint>


bool Recorder::add_samples(uint16_t *buffer, size_t count, uint16_t channels) {
    size_t copy_count = std::min(count, sample_array.size() - sample_idx);
    size_t b_idx = 0;
    size_t s_idx = sample_idx;
    size_t end_exidx = sample_idx + copy_count;
    int32_t offset = 32768 >> 4;    // TODO work out clean way to do shift

    if (channels == 1) {
        while (s_idx < end_exidx) {
            sample_array[s_idx++] = int16_t(buffer[b_idx++] - offset) << 4;  // TODO - work out clean way to do shift
        }
    } else if (channels == 2) {
        while (s_idx < end_exidx) {
            int16_t mono_sample = (buffer[b_idx++] + buffer[b_idx++]) >> 1;
            sample_array[s_idx++] = int16_t(mono_sample - offset) << 4;
        }
    }
    sample_idx = end_exidx;

    // indicate whether sample_array is full
    return end_exidx == sample_array.size();
}

bool Recorder::add_samples(int16_t *buffer, size_t count, uint16_t channels) {
    size_t copy_count = std::min(count, sample_array.size() - sample_idx);
    size_t b_idx = 0;
    size_t s_idx = sample_idx;
    size_t end_exidx = sample_idx + copy_count; 
    if (channels == 1) {
        memcpy(sample_array.data(), &buffer[b_idx], copy_count * sizeof(int16_t)); 
    } else if (channels == 2) {
        while (s_idx < end_exidx) {
            int16_t mono_sample = (buffer[b_idx++] + buffer[b_idx++]) >> 1;
            sample_array[s_idx++] = mono_sample;
        }
    }
    sample_idx = end_exidx;

    // indicate whether sample_array is full
    return end_exidx == sample_array.size();
}

void Recorder::process() {
    int samples_per_line = 8;  // This cannot be changed
 
    size_t idx = 0;
    size_t len = sample_array.size();
    printf("\n\n");
    size_t full_lines_len = len - len % samples_per_line;
    for (; idx < full_lines_len; idx += samples_per_line) {
        printf("%" PRId16 ", " "%" PRId16 ", " 
               "%" PRId16 ", " "%" PRId16 ", "
               "%" PRId16 ", " "%" PRId16 ", " 
               "%" PRId16 ", " "%" PRId16 ", \n",
               sample_array[idx], sample_array[idx + 1],
               sample_array[idx + 2], sample_array[idx + 3],
               sample_array[idx + 4], sample_array[idx + 5], 
               sample_array[idx + 6], sample_array[idx + 7]);
    };
    if (idx < len) {
        for(; idx < len; idx++) {
            printf("%" PRId16 ", ", sample_array[idx]); 
        }
        printf("\n");
    }

    sample_idx = 0;  // reset index as data has been processed
}

