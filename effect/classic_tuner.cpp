/**
 * Copyright (c) 2023 Kevin J. Walters
 *
 * SPDX-License-Identifier: MIT
 */


#include <cmath>

#include "pico_graphics.hpp"

#include "effect/effect.hpp"
#include "effect/lib/rgb.hpp"
#include "uad/debug.hpp"
#include "uad/display.hpp"


static constexpr int8_t MIDI_NOTE_A4 = 69;

static std::array<std::string, 12> note_names{"C", "C#", "D", "D#",
                                              "E", "F", "F#", "G",
                                              "G#", "A", "A#", "B"};

std::string midi_note_name(int8_t midi_note) {
    int8_t octave = midi_note / 12 - 1;
    int8_t note = midi_note % 12;
    return note_names[note] + std::to_string(octave);
}


void EffectClassicTuner::updateDisplay(void) {
    int8_t new_tuner_cursor_x = NOT_SHOWN;
    float frequency = tuner.frequency;
    float semitone, nearest_semitone, out_of_tune;
    bool pitch_perfect = false;
    if (frequency != 0.0f) {
        semitone = logf(frequency / Tuner::A4_REF_FREQUENCY) * semitone_conv;
        nearest_semitone = roundf(semitone);
        out_of_tune = semitone - nearest_semitone;
        // TODO - handle Cosmic display
        new_tuner_cursor_x = int8_t(roundf(out_of_tune * (MID_POS / 0.5f)) + MID_POS);
        pitch_perfect = new_tuner_cursor_x == MID_POS;
        tuner_text.text(midi_note_name(int8_t(nearest_semitone) + MIDI_NOTE_A4), 3);
    }
 
    if (new_tuner_cursor_x == tuner_cursor_x) {
        return;
    }

    if (tuner_cursor_x != NOT_SHOWN) {
        for (uint8_t y=1; y < Display::HEIGHT - 1; y++) {
            display.set_pixel(tuner_cursor_x, y,
                              0u, 0u, 0u);
        }
    }
    if (new_tuner_cursor_x != NOT_SHOWN) {
        for (uint8_t y=1; y < Display::HEIGHT - 1; y++) {
            if (pitch_perfect) {
                display.set_pixel(new_tuner_cursor_x, y,
                                  0u, 128u, 0u);
            } else {
                display.set_pixel(new_tuner_cursor_x, y,
                                  128u, 0u, 0u);
            }             
        }
    }
    tuner_cursor_x = new_tuner_cursor_x;
}

void EffectClassicTuner::start(void) {
    display.clear();    
    display.set_pixel(26, 0,
                      0, 64u, 0u);
    display.set_pixel(26, Display::HEIGHT - 1,
                      0, 64u, 0u);

    // default step is 1, other effects may have changed it
    tuner.setStep(2);
}

void EffectClassicTuner::init(void) {
    if (debug >=1) {
        printf("EffectClassicTuner: %ix%i\n", display.WIDTH, display.HEIGHT);
    }
}

