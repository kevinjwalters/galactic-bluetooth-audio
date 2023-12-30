/**
 * Copyright (c) 2023 Kevin J. Walters
 *
 * SPDX-License-Identifier: MIT
 */


#pragma once

#include <cstdint>
#include <cstddef>
#include <memory>
#include <string>
#include <vector>
#include <functional>

#include "pico.h"
#include "hardware/address_mapped.h"
#include "hardware/structs/pio.h"
#include "hardware/gpio.h"
#include "hardware/regs/dreq.h"
#include "hardware/pio_instructions.h"

#include "pico_graphics.hpp"
#include "bitmap_fonts.hpp"
#include "font8_data.hpp"

#include "uad/welcome.hpp"
#include "uad/display.hpp"
#include "uad/audiosource.hpp"
#include "uad/unicornbutton.hpp"

#include "effect/lib/fixed_fft.hpp"
#include "effect/lib/recorder.hpp"
#include "effect/lib/tuner.hpp"
#include "effect/effect.hpp"


class UnicornAudioDisplay {
  protected:
    static constexpr int16_t MAX_LOCAL_VOLUME = 128;
    static constexpr int32_t UAD_FONT_HEIGHT = 8;

    Display display;
    pimoroni::PicoGraphics_PenRGB888 graphics;
    std::vector<Effect *> effects;

    // RingBuffer buffer;  // TODO - this does not exist yet!

    FIX_FFT fft;
    Tuner tuner;
    // Recorder recorder;
    //EffectConsoleRecorder effect_console_recorder;
    EffectSpectrogramFFT effect_spectrogram_fft;
    EffectRainbowFFT effect_rainbow_fft;
    EffectClassicFFT effect_classic_fft;
    EffectClassicTuner effect_classic_tuner;
    int current_effect;
    UnicornButton button_a, button_b, button_c, button_d;
    UnicornButton button_volume_down, button_volume_up;
    UnicornButton button_brightness_down, button_brightness_up;
    bool init_okay;
    bool active;
    std::vector<std::shared_ptr<AudioSource>> sources;
    int current_source;
    std::array<int64_t, 100> effect_update_time;
    size_t eut_idx;
    const bitmap::font_t *uad_font_ptr;

    // The DMA ADC code uses GPIO27 which is "shared" with SLEEP/Zzz button
    // button_sleep(Display::SWITCH_SLEEP);  // DO NOT USE THIS

  public:
    int16_t display_y_scale = 100;  // 0..1000
    int16_t local_volume = MAX_LOCAL_VOLUME;  // 0..128

    UnicornAudioDisplay() :
        graphics(Display::WIDTH, Display::HEIGHT, nullptr),
        //effect_console_recorder(display, recorder),
        effect_spectrogram_fft(display, fft),
        effect_rainbow_fft(display, fft),
        effect_classic_fft(display, fft),
        effect_classic_tuner(display, tuner),
        current_effect(0),
        button_a(Display::SWITCH_A),
        button_b(Display::SWITCH_B),
        button_c(Display::SWITCH_C),
        button_d(Display::SWITCH_D),
        button_volume_down(Display::SWITCH_VOLUME_DOWN),
        button_volume_up(Display::SWITCH_VOLUME_UP),
        button_brightness_down(Display::SWITCH_BRIGHTNESS_DOWN),
        button_brightness_up(Display::SWITCH_BRIGHTNESS_UP),
        init_okay(true),
        active(false),
        current_source(0),
        eut_idx(0),
        sample_rate(0),
        uad_font_ptr(&font8) {
        //effects.push_back(&effect_console_recorder);
        effects.push_back(&effect_spectrogram_fft);
        effects.push_back(&effect_rainbow_fft);
        effects.push_back(&effect_classic_fft);
        effects.push_back(&effect_classic_tuner);
        effect_update_time = {};  // init to 0s

        for (auto &effect : effects) {
            effect->init();
        }
        effects[current_effect]->start();

        // 0 is audio_rate - this disables audio (I2S/PIO/DMA/IRQ code)
        // as I2S audio is handled in AudioSourceBluetooth
        // and things don't work if pio0 is used
        display.init(0, pio1);
        display.clear();

        graphics.set_font(uad_font_ptr);
    };

    void init(std::vector<std::shared_ptr<AudioSource>> &sources_) {
        sources = sources_;
#ifdef WELCOME1
        text(WELCOME1,
             1200,
             [this](void) { return button_a.pressed(); });
#endif
#ifdef WELCOME2
        text(WELCOME2,
             1200,
             [this](void) { return button_a.pressed(); });
#endif
#ifdef WELCOME3
        text(WELCOME3,
             1200,
             [this](void) { return button_a.pressed(); });
#endif
        sources[current_source]->init();
        uint32_t rate = sources[current_source]->getSampleRate();
        if (rate > 0) {
            setSampleRate(rate);
        }
    };

    void run() { sources[current_source]->run(); };
    void deinit() { sources[current_source]->deinit();};
    bool okay() { return sources[current_source]->okay(); };

    void processData(uint16_t *buffer_u, size_t sample_count, uint16_t channels) {
        absolute_time_t effect_update_post;
        absolute_time_t effect_update_pre = get_absolute_time();
        effects[current_effect]->update(buffer_u, sample_count, channels);
        effect_update_post = get_absolute_time();

        updatePerfStats(absolute_time_diff_us(effect_update_pre,
                                              effect_update_post));
    }

    void processData(int16_t *buffer_s, size_t sample_count, uint16_t channels) {
        absolute_time_t effect_update_post;
        absolute_time_t effect_update_pre = get_absolute_time();
        effects[current_effect]->update(buffer_s, sample_count, channels);
        effect_update_post = get_absolute_time();

        updatePerfStats(absolute_time_diff_us(effect_update_pre,
                                              effect_update_post));
    }

    void checkButtons(void) {
        absolute_time_t now_t = get_absolute_time();

        // Button A: next effect
        if (button_a.pressedSince(now_t)) {
            effects[current_effect]->stop();

            current_effect = (current_effect + 1) % effects.size();
            // Show the name of the new effect
            text(effects[current_effect]->display_name,
                 1200,
                 [this](void) { return button_a.pressed(); });
            effects[current_effect]->start();
            while (button_a.pressed()) {
                // wait for button release
            }
        }

        // Button B: pass to Effect
        if (button_b.pressedSince(now_t)) {
            effects[current_effect]->buttonB();
        }

        // Button C: pass to Effect and currently the next FFT scale
        if (button_c.pressedSince(now_t)) {
            effects[current_effect]->buttonC(); // TODO - finish off moving scale change to effects
            fft.next_scale();  // TODO - would be better to call an effect method and let it handle this
        }

        // Button D: shift button to change volume to scale adjustment
        if (button_d.pressed()) {
            if (button_volume_down.pressedSince(now_t)) {
                if (display_y_scale >= 200) {
                    display_y_scale -= 100;
                } else if (display_y_scale >= 10) {
                    display_y_scale -= 10;
                }
            } else if (button_volume_up.pressedSince(now_t)) {
                if (display_y_scale <= 90) {
                    display_y_scale += 10;
                } else if (display_y_scale <= 900) {
                    display_y_scale += 100;
                }
            }
        }

        if (button_volume_down.pressedSince(now_t) && local_volume >= 16) {
            local_volume -= 16;
        } else if (button_volume_up.pressedSince(now_t) && local_volume <= MAX_LOCAL_VOLUME - 16) {
            local_volume += 16;
        }
    }

    uint32_t getSampleRate(void) {
        return sample_rate;
    }
    void setSampleRate(uint32_t rate) {
        if (rate != sample_rate) {
            sample_rate = rate;
            tuner.setSampleRate(sample_rate);
            fft.setSampleRate(sample_rate);
            for (auto &effect : effects) {
                effect->setSampleRate(sample_rate);
            }
        }
    }

  private:
    uint32_t sample_rate;

    void updatePerfStats(int64_t duration) {
        effect_update_time[eut_idx++] = duration;
        if (eut_idx == effect_update_time.size()) {
            // printf("PROFILE ");
            // This takes ages to print and messed up DMA ADC collection
            // disable for now
            // for (size_t ii = 0; ii < eut_idx; ii++) {
            //   printf("%lld ", effect_update_time[ii]);
            // }
            // TODO - replace with compact print out of statistics
            // /printf("\n");
            eut_idx = 0;
        }
    }

    void fillDisplay(uint8_t r, uint8_t g, uint8_t b) {
        for (uint8_t y=0; y < Display::HEIGHT; y++) {
            for (uint8_t x=0; x < Display::WIDTH; x++) {
                display.set_pixel(x, y, r, g, b);
            }
        }
    }

    void clearDisplay(void) {
        fillDisplay(0u, 0u, 0u);
    }

    bool sleep_ms_with_skip(int32_t delay, std::function<bool()> skip_func, uint32_t step_ms = 50) {
        // Do a normal sleep if there's no skip function
        if (skip_func == nullptr) {
            sleep_ms(delay);
            return false;
        }

        int32_t countdown_ms = delay;
        bool terminate = false;
        while (!terminate && countdown_ms > 0) {
            sleep_ms(step_ms);
            countdown_ms -= step_ms;
            terminate = skip_func();
        }
        return terminate;
    }

    void text(const std::string &msg, int32_t showtime_ms = 1200, std::function<bool()> skip_func = nullptr, char pos = 'c') {
        std::string padded_msg = msg;
        bool skip = false;
        int32_t width = bitmap::measure_text(uad_font_ptr, padded_msg, 1, 1, false);
        int32_t scroll_pixels = width - Display::WIDTH;

        // Make text that needs scrolling easier to read by starting it not near the edge
        if (scroll_pixels > 0) {
            padded_msg = " " + padded_msg;
            width = bitmap::measure_text(uad_font_ptr, padded_msg, 1, 1, false);
            scroll_pixels = width - Display::WIDTH;
        }

        int32_t x_pos = (Display::WIDTH - width) / 2;
        x_pos = (x_pos < 0) ? 0 : x_pos;
        // The +1 here makes the font8 look better on 11 pixel high Unicorn
        int32_t y_pos = (Display::HEIGHT - UAD_FONT_HEIGHT) / 2 + 1;

        graphics.set_pen(0, 0, 0);
        graphics.clear();
        graphics.set_pen(128, 0, 0);
        graphics.text(padded_msg, pimoroni::Point(x_pos, y_pos), -1, 1.0);
        display.update(&graphics);

        // Scroll the text if this is necessary
        if (scroll_pixels > 0) {
            // extra pauses to aid reading for scrolling
            // don't check skip_func here to allow use to lift finger off button
            sleep_ms(800);

            for (int32_t idx=0; idx < scroll_pixels; idx++) {
                graphics.set_pen(0, 0, 0);
                graphics.clear();
                graphics.set_pen(128, 0, 0);
                graphics.text(msg, pimoroni::Point(x_pos - idx, y_pos), -1, 1.0);
                display.update(&graphics);
                skip = sleep_ms_with_skip(100, skip_func);
                if (skip) { break; };
            }
            if (skip) { return; };
            // extra pauses to aid reading for scrolling
            skip = sleep_ms_with_skip(400 + showtime_ms, skip_func);
            if (skip) { return; };
        } else {
            // don't check skip_func here to allow use to lift finger off button
            if (showtime_ms > 800) {
                sleep_ms(800);
                sleep_ms_with_skip(showtime_ms - 800, skip_func);
            } else {
                sleep_ms(showtime_ms);
            }
        }
    }
};

