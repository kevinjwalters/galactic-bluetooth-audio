/**
 * Copyright (c) 2023 Pimoroni Ltd <phil@pimoroni.com>
 * Copyright (c) 2023 V. Hunter Adams <vha3@cornell.edu>
 * Copyright (c) 2023 Kevin J. Walters
 *
 * SPDX-License-Identifier: MIT
 *
 * Reproduced and modified with explicit permission
 *
 * Original code in action: https://www.youtube.com/watch?v=8aibPy4yzCk
 */

#include "fixed_fft.hpp"
#include <algorithm>

// Adapted from https://github.com/raspberrypi/pico-sdk/blob/master/src/host/pico_bit_ops/bit_ops.c
uint16_t __always_inline __revs(uint16_t v) {
    v = ((v & 0x5555u) << 1u) | ((v >> 1u) & 0x5555u);
    v = ((v & 0x3333u) << 2u) | ((v >> 2u) & 0x3333u);
    v = ((v & 0x0f0fu) << 4u) | ((v >> 4u) & 0x0f0fu);
    return ((v >> 8u) & 0x00ffu) | ((v & 0x00ffu) << 8u);
}

FIX_FFT::~FIX_FFT() {
}

int FIX_FFT::get_scaled(unsigned int i) {
    return fix15_to_int(multiply_fix15(fr[i], loudness_adjust[i]));
}

fix15 FIX_FFT::get_scaled_as_fix15(unsigned int i) {
    return multiply_fix15(fr[i], loudness_adjust[i]);
}


void FIX_FFT::setSampleRate(uint32_t rate) {
    if (sample_rate != rate) {
        sample_rate = rate;
        sample_rate_f = float(rate);
        set_scale(scale);
    }
}

void FIX_FFT::init() {
    // Populate Filter and Sine tables
    for (auto ii = 0u; ii < SAMPLE_COUNT; ii++) {
        // Full sine wave with period NUM_SAMPLES
        // Wolfram Alpha: Plot[(sin(2 * pi * (x / 1.0))), {x, 0, 1}]
        sine_table[ii] = float_to_fix15(0.5f * sinf((M_PI * 2.0f) * ((float) ii) / (float)SAMPLE_COUNT));

        // This is a crude approximation of a Lanczos window.
        // Wolfram Alpha Comparison: Plot[0.5 * (1.0 - cosf(2 * pi * (x / 1.0))), {x, 0, 1}], Plot[LanczosWindow[x - 0.5], {x, 0, 1}]
        filter_window[ii] = float_to_fix15(0.5f * (1.0f - cosf((M_PI * 2.0f) * ((float) ii) / ((float)SAMPLE_COUNT))));
    }
}

void FIX_FFT::next_scale(void) {
    (void)contour.next();
    set_scale(scale);
}

void FIX_FFT::set_scale(float new_scale) {
    scale = new_scale;

    // Set DC offset
    loudness_adjust[0] = float_to_fix15(scale / float(SAMPLE_COUNT));

    const LoudnessLookup *lookup = contour.lookup();
    // Only set 1 to N/2+1 buckets, rest are mirror image for a real world
    float multiplier = 2.0f * scale / float(SAMPLE_COUNT);
    float freq_step_f = sample_rate_f / SAMPLE_COUNT;
    float freq_f = freq_step_f;
    for (int i = 1; i < SAMPLE_COUNT / 2 + 1; ++i) {
        int freq = int(roundf(freq_f));
        int j = 0;
        while (lookup[j+1].freq < freq) {
            ++j;
        }
        float t = float(freq - lookup[j].freq) / float(lookup[j+1].freq - lookup[j].freq);
        loudness_adjust[i] = float_to_fix15(multiplier * (t * lookup[j+1].multiplier +
                                                          (1.0f - t) * lookup[j].multiplier));
        freq_f += freq_step_f;
    }
}


bool FIX_FFT::add_samples(uint16_t *buffer, size_t count, uint16_t channels) {
    size_t copy_count = std::min(count, sizeof(sample_array) / sizeof(sample_array[0]) - sample_idx);
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
    return end_exidx == sizeof(sample_array) / sizeof(sample_array[0]);
}


bool FIX_FFT::add_samples(int16_t *buffer, size_t count, uint16_t channels) {
    size_t copy_count = std::min(count, sizeof(sample_array) / sizeof(sample_array[0]) - sample_idx);
    size_t b_idx = 0;
    size_t s_idx = sample_idx;
    size_t end_exidx = sample_idx + copy_count; 
    if (channels == 1) {
        memcpy(sample_array, &buffer[b_idx], copy_count * sizeof(int16_t));
    } else if (channels == 2) {
        while (s_idx < end_exidx) {
            int16_t mono_sample = (buffer[b_idx++] + buffer[b_idx++]) >> 1;
            sample_array[s_idx++] = mono_sample;
        }
    }
    sample_idx = end_exidx;

    // Original weird code
    // int16_t* fft_array = &sample_array[SAMPLES_PER_AUDIO_BUFFER * (BUFFERS_PER_FFT_SAMPLE - 1)];
    // memmove(sample_array, &sample_array[SAMPLES_PER_AUDIO_BUFFER], (BUFFERS_PER_FFT_SAMPLE - 1) * sizeof(uint16_t));
    // for (auto i = 0u; i < SAMPLES_PER_AUDIO_BUFFER; i++) {
    //     fft_array[i] = buffer[i];
    // }

    // indicate whether sample_array is full
    return end_exidx == sizeof(sample_array) / sizeof(sample_array[0]);
}

void FIX_FFT::process() {
    float max_freq = 0.0f;

    // Copy/window elements into a fixed-point array
    for (auto i = 0u; i < SAMPLE_COUNT; i++) {
        fr[i] = multiply_fix15(int_to_fix15((int)sample_array[i]), filter_window[i]);
        fi[i] = (fix15)0;
    }

    // Compute the FFT
    FFT();

    // Find the magnitudes
    for (auto i = 0u; i < (SAMPLE_COUNT / 2u); i++) {
        // get the approx magnitude using approximated sqrt()
        // and store in fr
        fix15 abs_re = abs(fr[i]);
        fix15 abs_im = abs(fi[i]);
        fr[i] = std::max(abs_re, abs_im) + 
                multiply_fix15(std::min(abs_re, abs_im), float_to_fix15(0.4f)); 

        // Keep track of maximum frequency
        if (fr[i] > max_freq && i >= 5u) {
            max_freq = fr[i];
            max_freq_dex = i;
        }
    }

    sample_idx = 0;  // reset index as data has been processed
}

float FIX_FFT::max_frequency() {
    return max_freq_dex * (sample_rate_f / SAMPLE_COUNT);
}

void FIX_FFT::FFT() {
    // Bit Reversal Permutation
    // Bit reversal code below originally based on that found here: 
    // https://graphics.stanford.edu/~seander/bithacks.html#BitReverseObvious
    // https://en.wikipedia.org/wiki/Bit-reversal_permutation
    // Detail here: https://vanhunteradams.com/FFT/FFT.html#Single-point-transforms-(reordering)
    //
    // PH: Converted to stdlib functions and __revs so it doesn't hurt my eyes
    for (auto m = 1u; m < SAMPLE_COUNT - 1u; m++) {
        unsigned int mr = __revs(m) >> shift_amount;
        // don't swap that which has already been swapped
        if (mr <= m) continue;
        // swap the bit-reveresed indices
        std::swap(fr[m], fr[mr]);
        // std::swap(fi[m], fi[mr]); // For our special case this isn't needed as all zero
    }

    // Danielson-Lanczos
    // Adapted from code by:
    // Tom Roberts 11/8/89 and Malcolm Slaney 12/15/94 malcolm@interval.com
    // Detail here: https://vanhunteradams.com/FFT/FFT.html#Two-point-transforms
    // Length of the FFT's being combined (starts at 1)
    //
    // PH: Moved variable declarations to first-use so types are visually explicit.
    // PH: Removed div 2 on sine table values, have computed the sine table pre-divided.
    unsigned int L = 1;
    int k = log2_samples - 1;

    // While the length of the FFT's being combined is less than the number of gathered samples
    while (L < SAMPLE_COUNT) {
        // Determine the length of the FFT which will result from combining two FFT's
        int istep = L << 1;
        // For each element in the FFT's that are being combined
        for (auto m = 0u; m < L; ++m) { 
            // Lookup the trig values for that element
            int j = m << k; // index into sine_table
            fix15 wr =  sine_table[j + SAMPLE_COUNT / 4];
            fix15 wi = -sine_table[j];
            // i gets the index of one of the FFT elements being combined
            for (auto i = m; i < SAMPLE_COUNT; i += istep) {
                // j gets the index of the FFT element being combined with i
                int j = i + L;
                // compute the trig terms (bottom half of the above matrix)
                fix15 tr = multiply_fix15_unit(wr, fr[j]) - multiply_fix15_unit(wi, fi[j]);
                fix15 ti = multiply_fix15_unit(wr, fi[j]) + multiply_fix15_unit(wi, fr[j]);
                // divide ith index elements by two (top half of above matrix)
                fix15 qr = fr[i] >> 1;
                fix15 qi = fi[i] >> 1;
                // compute the new values at each index
                fr[j] = qr - tr;
                fi[j] = qi - ti;
                fr[i] = qr + tr;
                fi[i] = qi + ti;
            }    
        }
        --k;
        L = istep;
    }
}
