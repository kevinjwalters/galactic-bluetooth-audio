/**
 * Copyright (c) 2018 Joel de Guzman
 * Copyright (c) 2018 Gerry Beauregard
 * Copyright (c) 2023 Kevin J. Walters
 * 
 * SPDX-License-Identifier: MIT
 *
 * This is mostly https://github.com/cycfi/bitstream_autocorrelation/blob/master/bcf2.cpp
 * with some code based on work by Gerry Beauregard
 * https://web.archive.org/web/20150725154904/https://gerrybeauregard.wordpress.com/2013/07/15/high-accuracy-monophonic-pitch-estimation-using-normalized-autocorrelation/
 */

#pragma once

#include <cstdint>
#include <vector>


template <typename T = uint32_t>
struct Bitstream
{
    static_assert(std::is_unsigned<T>::value, "T must be unsigned");
    static constexpr auto nbits = 8 * sizeof(T);
 
    Bitstream(size_t size_)
    {
        // size = smallest_pow2(size_);   // TODO - why is this a power of 2
        // array_size = size / nbits;
 
        requested_size = size_;
        array_size = divide_roundup(requested_size, nbits);
        size = array_size * nbits;
        bits.resize(array_size, 0);
    }
 
    size_t divide_roundup(size_t dd, size_t dr)
    {
        return (dd + dr - 1) / dr;  // works for positive integers
    }
 
    static constexpr T smallest_pow2(T n, T m = 1)
    {
        return (m < n)? smallest_pow2(n, m << 1) : m;
    }
 
    static uint32_t count_the_bits(uint32_t i)
    {
        // GCC only!!!
        return __builtin_popcount(i);
    }
 
    void clear()
    {
        std::fill(bits.begin(), bits.end(), 0);
    }
 
    void set(uint32_t i, bool val)
    {
        auto mask = 1 << (i % nbits);
        auto& ref = bits[i / nbits];
        ref ^= (-T(val) ^ ref) & mask;
    }
 
    bool get(uint32_t i) const
    {
        auto mask = 1 << (i % nbits);
        return (bits[i / nbits] & mask) != 0;
    }
 
    template <typename F>
    void auto_correlate(size_t start_pos, F f)
    {
        auto mid_array = (array_size / 2) - 1;
        auto mid_pos = size / 2;
        auto index = start_pos / nbits;
        auto shift = start_pos % nbits;
  
        for (auto pos = start_pos; pos != mid_pos; ++pos)
        {
            auto* p1 = bits.data();
            auto* p2 = bits.data() + index;
            auto count = 0;
   
            if (shift == 0)
            {
                for (auto i = 0; i != mid_array; ++i)
                    count += count_the_bits(*p1++ ^ *p2++);
            }
            else
            {
                auto shift2 = nbits - shift;
                for (auto i = 0; i != mid_array; ++i)
                {
                    auto v = *p2++ >> shift;
                    v |= *p2 << shift2;
                    count += count_the_bits(*p1++ ^ v);
                }
            }
            ++shift;
            if (shift == nbits)
            {
                shift = 0;
                ++index;
            }
   
            f(pos, count);
        }
    }
 
    std::vector<T> bits;
    size_t requested_size;
    size_t size;
    size_t array_size;
};

