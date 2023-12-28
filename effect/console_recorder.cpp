/**
 * Copyright (c) 2023 Kevin J. Walters
 *
 * SPDX-License-Identifier: MIT
 */


#include "effect.hpp"

#include "effect/lib/rgb.hpp"
#include "uad/debug.hpp"


void EffectConsoleRecorder::updateDisplay(void) {
  
}

void EffectConsoleRecorder::init(void) {
    if (debug >=1) {
        printf("EffectConsoleRecorder: %ix%i\n", display.WIDTH, display.HEIGHT);
    }
}

