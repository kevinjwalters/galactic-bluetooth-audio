/**
 * Copyright (c) 2023 Kevin J. Walters
 *
 * SPDX-License-Identifier: MIT
 */


#pragma once

#include "pico/stdlib.h"
#include "pico/time.h"
#include "hardware/gpio.h"

#include "uad/display.hpp"


class UnicornButton {
  private:
    uint gpio;
    absolute_time_t time_last;
    int button_repeat_ms;

  public:
    explicit UnicornButton(uint gpio) : gpio(gpio),
                                        time_last(get_absolute_time()),
                                        button_repeat_ms(250) {};
    bool pressed() { return !gpio_get(gpio); };
    bool pressedSince(absolute_time_t now) {
        bool pressed = !gpio_get(gpio) && absolute_time_diff_us(time_last, now) > 0;
        if (pressed) {
            time_last = delayed_by_ms(now, button_repeat_ms);
        }
        return pressed;
    };
};

