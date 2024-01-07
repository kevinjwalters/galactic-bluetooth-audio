/**
 * Copyright (c) 2018 Joel de Guzman
 * Copyright (c) 2023 Kevin J. Walters
 *
 * SPDX-License-Identifier: MIT
 *
 * some of this is "loosely used some of the support code by Gerry Beauregard"
 */


#include <cstdint>

#include "tuner.hpp"
#include "uad/debug.hpp"


class ZeroCross {
   public:
     ZeroCross(int16_t threshold) : pos_threshold(threshold),
                                    neg_threshold(-threshold),
                                    positive(false) {};

     bool operator()(int16_t value) {
         if (value < neg_threshold) {
             positive = false;
         } else if (value > pos_threshold) {
             positive = true;
         } else {
             // retain previous value
         }
         return positive;
     }

  private: 
     int16_t pos_threshold;
     int16_t neg_threshold;
     bool positive;
};


// TODO - rewrite for std::array ensuring caution on overflows
bool Tuner::add_samples(uint16_t *buffer, size_t count, uint16_t channels) {
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

bool Tuner::add_samples(int16_t *buffer, size_t count, uint16_t channels) {
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

void Tuner::process() {
    // Set the zero-crossing points in the Bitstream
    ZeroCross zc(zc_noise_magn);
    for (size_t i = 0; i != sample_idx; i++) {
        bitstream.set(i, zc(sample_array[i]));
    }

    uint32_t max_count = 0;
    uint32_t min_count = UINT32_MAX;
    size_t est_index = 0;
    // std::array<uint32_t,SAMPLE_COUNT_TUNER / 2> corr{};  // note {} here initializes as 0s
    std::vector<uint32_t> corr(SAMPLE_COUNT_TUNER / 2, 0);
  
    bitstream.auto_correlate(size_t(min_period),
        [&corr, &max_count, &min_count, &est_index](auto pos, auto count)
        {
            corr[pos] = count;
            max_count = std::max<uint32_t>(max_count, count);
            if (count < min_count)
            {
                min_count = count;
                est_index = pos;
            }
        }
    ); 

    size_t est_int_period = est_index; 
    float third_est_freq = 0.0f;
    int max_div = int(est_index / min_period);
    uint32_t sub_threshold = uint32_t(COUNT_CORR_SUB_THR_FRAC * max_count);
    
    // Look for lack of strong correlation
    if (min_count > COUNT_CORR_THR_MIN || max_count < COUNT_CORR_THR_MAX) {
        frequency = 0.0f;
        goto tidyup_and_return;
    }
 
    // Handle harmonics
    for (int div = max_div; div != 0; div--) {
        bool all_strong = true;
        float mul = 1.0f / div;

        for (int k = 1; k != div; k++) {
            size_t sub_period = (size_t)roundf(k * est_int_period * mul);
            if (corr[sub_period] > sub_threshold) {
                all_strong = false;
                break;
            }
        }

        if (all_strong) {
            est_int_period = (size_t)roundf(est_int_period * mul);
            break;
        }
    }

    // Estimate the pitch
    // Zero crossing interpolation code removed here 
 
    frequency = calculateFrequency(est_int_period);

  tidyup_and_return:
    if (debug >= 2) {
        float first_est_freq = est_index != 0 ? sample_rate_f / float(est_index) : 0.0f;
        float second_est_freq = est_int_period != 0 ? sample_rate_f / float(est_int_period) : 0.0f;
        printf("DEBUG: estimates 1st: %f  2nd: %f  3rd: %f  4th: %f\n",
               first_est_freq, second_est_freq, third_est_freq, frequency);
    }

    sample_idx = 0;  // reset index as data has been processed
}

float Tuner::calculateFrequency(size_t latest_int_period) {

    // Store value
    period_results[result_idx++] = latest_int_period;
    if (result_idx >= period_results.size()) { result_idx = 0; };

    latest_frequency = latest_int_period != 0 ? sample_rate / float(latest_int_period) : 0.0f;

    // Calculate arithemetic mean of non-zero values excluding
    // min&max outliers (for more outliers a sort would be needed)
    int32_t min_per = INT16_MAX, max_per = INT16_MIN;
    int32_t min_idx = -1, max_idx = -1;
    uint32_t sum = 0;
    int32_t values = 0;
    for (size_t idx = 0; idx < period_results.size(); idx++) {
        auto period = period_results[idx];
        // 0 values are a dont know
        if (period > 0) {
            sum += period;
            values++;
        }
        if (period <= min_per) { min_per = period; min_idx = idx; };
        if (period >= max_per) { max_per = period; max_idx = idx; };
    }
    // remove outliers
    if (min_idx >= 0) {
        sum -= period_results[min_idx];
        --values;
    };
    if (max_idx >= 0 && max_idx != min_idx) {
        sum -= period_results[max_idx];
        --values;
    };

    return (sum != 0 && values > 0) ? sample_rate * values / float(sum) : 0.0f;
}

void Tuner::setSampleRate(uint32_t rate) {
    if (sample_rate != rate) {
        sample_rate = rate;
        sample_rate_f = float(rate);
        min_period = sample_rate_f / MAX_FREQUENCY;
        max_period = sample_rate_f / MIN_FREQUENCY;
    }
}

