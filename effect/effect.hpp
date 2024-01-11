/**
 * Copyright (c) 2023 Pimoroni Ltd <phil@pimoroni.com>
 * Copyright (c) 2023 Kevin J. Walters
 *
 * SPDX-License-Identifier: MIT
 */


#pragma once

#include <array>
#include <cmath>
#include <cstdint>
#include <cstddef>
#include <functional>
#include <string>
#include "limits.h"


#include "pico_graphics.hpp"
#include "bitmap_fonts.hpp"
#include "font6_data.hpp"
#include "font8_data.hpp"

#include "uad/display.hpp"
#include "effect/lib/fixed_fft.hpp"
#include "effect/lib/recorder.hpp"
#include "effect/lib/tuner.hpp"
#include "effect/lib/rgb.hpp"


class Effect {
  public:
    Display &display;
    const std::string display_name;
    Effect(Display& display,
           const std::string &display_name) :
           display(display),
           display_name(display_name) { };
    virtual ~Effect() { };

    virtual void init(void);
    virtual void setSampleRate(uint32_t);
    virtual void update(int16_t *buffer16, size_t sample_count, uint16_t channels);
    virtual void update(uint16_t *buffer16, size_t sample_count, uint16_t channels);
    virtual void start(void);
    virtual void stop(void);
    virtual void buttonB(void);
    virtual void buttonC(void);

  protected:
    virtual void updateDisplay(void) = 0;
};


class EffectFFT : public Effect {
  public:
    FIX_FFT &fft;
    enum ScaleType {
        LOGARITHMIC,
        SQRT,
        LINEAR
    };
    ScaleType scale_type;
    EffectFFT(Display& display,
              const std::string &display_name,
              FIX_FFT& fft,
              ScaleType scale_type = LOGARITHMIC) :
         Effect(display, display_name),
         fft(fft),
         scale_type(scale_type) {};
    virtual ~EffectFFT() { };

    void update(int16_t *buffer16, size_t sample_count, uint16_t channels) override;
    void update(uint16_t *buffer16, size_t sample_count, uint16_t channels) override;

  protected:
    static int find_level(fix15 *levels, size_t count, fix15 value);
    virtual void updateDisplay(void) = 0;
};


class EffectTuner : public Effect {
  public:
    EffectTuner(Display& display,
                const std::string &display_name,
                Tuner& tuner) : Effect(display,
                                       display_name),
                                tuner(tuner) { };
    virtual ~EffectTuner() { };
    void update(int16_t *buffer16, size_t sample_count, uint16_t channels) override;
    void update(uint16_t *buffer16, size_t sample_count, uint16_t channels) override;

  protected:
    Tuner &tuner;
    virtual void updateDisplay(void) = 0;
};


class EffectText {
  public:
    EffectText(Display &display,
               const bitmap::font_t &bitmap_font,
               uint8_t text_r,
               uint8_t text_g,
               uint8_t text_b,
               int32_t text_pos_x = 0,
               int32_t text_pos_y = 0 ,
               char justification = 'l',
               bool background = true,
               uint8_t textbg_r = 0,
               uint8_t textbg_g = 0,
               uint8_t textbg_b = 0) :
        display(display),
        bitmap_font(bitmap_font),
        text_pos_x(text_pos_x),
        text_pos_y(text_pos_y),
        text_r(text_r),
        text_g(text_g),
        text_b(text_b),
        justification(justification),
        background(background),
        textbg_r(textbg_r),
        textbg_g(textbg_g),
        textbg_b(textbg_b) { };

    void rectangle(int32_t x, int32_t y, int32_t w, int32_t h,
                   uint8_t r, uint8_t g, uint8_t b) {
        for (int32_t yi = y; yi < y + h; yi++) {
            for (int32_t xi = x; xi < x + w; xi++) {
                display.set_pixel(xi, yi, r, g, b);
            }
        }
    };
    void text(const std::string &text, size_t fixed_len = 0) {
        // PicoGraphics_PenRGB888::text(msg, text_origin, -1, 1.0f);
        size_t string_len = fixed_len > 0 ? fixed_len : text.length();
        int32_t space_pixels = string_len > 1 ? (string_len - 1) * 1 : 0;
        int32_t tx = text_pos_x, y = text_pos_y;
        if (justification == 'r') {
            // Set x position for right justified text
            tx = text_pos_x - measure_text(&bitmap_font, text, SCALE, 1, false);
        }
        if (background) {
            int32_t bg_max_width_px = bitmap_font.max_width * string_len + space_pixels;
            int32_t bx = (justification == 'r') ? text_pos_x - bg_max_width_px : text_pos_x;
            rectangle(bx, y,
                      bg_max_width_px, bitmap_font.height,
                      textbg_r, textbg_g, textbg_b);
        }
        bitmap::text(&bitmap_font,
                     [this](int32_t x, int32_t y, int32_t w, int32_t h) { rectangle(x, y,
                                                                                    w, h,
                                                                                    text_r, text_g, text_b); },
                     text,
                     tx, y,
                     -1, SCALE, 1, false, 0);
    };

  private:
    constexpr static const uint8_t SCALE = 1;

    Display &display;
    const bitmap::font_t &bitmap_font;
    int32_t text_pos_x, text_pos_y;
    uint8_t text_r, text_g, text_b;
    char justification;
    bool background;
    uint8_t textbg_r, textbg_g, textbg_b;
};


class EffectClassicTuner : public EffectTuner {
  public:
    EffectClassicTuner(Display& display,
                       Tuner& tuner) : EffectTuner(display, "Tuner", tuner),
                                       tuner_cursor_x(-1),
                                       semitone_conv(12.0f / logf(2.0f)),
                                       tuner_text(display, font8, 20, 20, 64) { };
    virtual ~EffectClassicTuner() { };

  protected:
    constexpr static int8_t NOT_SHOWN = -1;
    constexpr static int8_t MID_POS = (Display::WIDTH - 1) / 2;

    EffectText tuner_text;
    int8_t tuner_cursor_x;  // -1 used for not shown
    float semitone_conv;    // (1 / log(2)) * 12

    void updateDisplay(void) override;
    void start(void) override;
    void init(void) override;
};


class EffectAudioscopeTuner : public EffectTuner {
  public:
    EffectAudioscopeTuner(Display& display,
                          Tuner& tuner) : EffectTuner(display, "Audioscope", tuner),
                                          freq_text(display, font6,
                                                    64, 64, 64, 
                                                    Display::WIDTH, 
                                                    Display::HEIGHT - font6.height,
                                                    'r', false),
                                          // traces initialised in start()
                                          trace_intensity({8, 20, 51, 128}),
                                          trace_idx(0) { };
    virtual ~EffectAudioscopeTuner() { };

  protected:
    void updateDisplay(void) override;
    void start(void) override;
    void init(void) override;

  private:
    constexpr static int8_t Y_MID_POS = (Display::HEIGHT - 1) / 2;
    constexpr static float  y_scale   = float(Display::HEIGHT) / (1 - 2 * INT16_MIN);
    constexpr static int8_t OFF_SCREEN = INT8_MIN;
    constexpr static size_t TRACE_NUM = 4;

    EffectText freq_text;
    std::array<std::array<int8_t, Display::WIDTH>, TRACE_NUM> traces;
    std::array<uint8_t, TRACE_NUM> trace_intensity;
    size_t trace_idx;  // indicates oldest trace
  
    void drawTraces(void);
};


class EffectSpectrogramFFT : public EffectFFT {
    private:
        // Number of FFT bins to skip on the left, the low frequencies tend to be pretty boring visually
        static constexpr unsigned int FFT_SKIP_BINS = 1;

        static constexpr int UPDATES_PER_SCROLL = 10;
        static constexpr fix15 mean_multiplier = float_to_fix15(1.0f / float(UPDATES_PER_SCROLL));
        fix15 sample_total[Display::WIDTH];
        uint16_t t_sample_count;

        RGB palette[Display::LEVELS];
        fix15 brightness_levels[Display::LEVELS];

        void init_totals(void);

    protected:
        void updateDisplay(void) override;

    public:
        EffectSpectrogramFFT(Display& display,
                             FIX_FFT& fft) : EffectFFT(display,
                                                       "Spectrogram",
                                                       fft) {};
        virtual ~EffectSpectrogramFFT() { };
        void init(void) override;
        void start(void) override;
};

class EffectRainbowFFT : public EffectFFT {
    private:
        // Number of FFT bins to skip on the left, the low frequencies tend to be pretty boring visually
        static constexpr unsigned int FFT_SKIP_BINS = 1;
        static constexpr int HISTORY_LEN = 21; // About 0.25s
        uint history_idx;
        uint8_t eq_history[Display::WIDTH][HISTORY_LEN];

        RGB palette_peak[Display::WIDTH];
        RGB palette_main[Display::WIDTH];

        fix15 y_ticks[Display::HEIGHT];

        float max_sample_from_fft;
        int lower_threshold;
#ifdef SCALE_LOGARITHMIC
        fix15 multiple;
#elif defined(SCALE_SQRT)
        fix15 subtract_step;
#elif defined(SCALE_LINEAR)
        fix15 subtract;
#else
#error "Choose a scale mode"
#endif

    protected:
        void updateDisplay(void) override;

    public:
        EffectRainbowFFT(Display& display,
                         FIX_FFT& fft) : EffectFFT(display,
                                                   "Rainbow Spectrum Analyser",
                                                   fft) {};
        virtual ~EffectRainbowFFT() { };
        void init(void) override;
        void start(void) override;
};


class EffectClassicFFT : public EffectFFT {
    private:
        // Number of FFT bins to skip on the left, the low frequencies tend to be pretty boring visually
        static constexpr unsigned int FFT_SKIP_BINS = 1;
        static constexpr int HISTORY_LEN = 21; // About 0.25s
        uint history_idx;
        uint8_t eq_history[Display::WIDTH][HISTORY_LEN];

        RGB palette[Display::HEIGHT];

        fix15 y_ticks[Display::HEIGHT];

        float max_sample_from_fft;
        int lower_threshold;
#ifdef SCALE_LOGARITHMIC
        fix15 multiple;
#elif defined(SCALE_SQRT)
        fix15 subtract_step;
#elif defined(SCALE_LINEAR)
        fix15 subtract;
#else
#error "Choose a scale mode"
#endif

    protected:
        void updateDisplay(void) override;

    public:
        EffectClassicFFT(Display& display,
                         FIX_FFT &fft) : EffectFFT(display,
                                         "Spectrum analyser",
                                         fft) {};
        virtual ~EffectClassicFFT() { };
        void init(void) override;
        void start(void) override;
};


class EffectRecorder : public Effect {
  public:
    EffectRecorder(Display& display,
                   const std::string &display_name,
                   Recorder& recorder) : Effect(display, display_name),
                                         recorder(recorder),
                                         recording(false) { };
    virtual ~EffectRecorder() { };
    void update(int16_t *buffer16, size_t sample_count, uint16_t channels) override;
    void update(uint16_t *buffer16, size_t sample_count, uint16_t channels) override;
    void buttonB(void) override;

  protected:
    Recorder &recorder;
    bool recording;
    virtual void updateDisplay(void) = 0;
};


class EffectConsoleRecorder : public EffectRecorder {
  public:
    EffectConsoleRecorder(Display& display,
                          Recorder& recorder) : EffectRecorder(display,
                                                               "Sample recorder",
                                                               recorder) {};
    virtual ~EffectConsoleRecorder() { };

  protected:
    void updateDisplay(void) override;
    void init(void) override;
};

