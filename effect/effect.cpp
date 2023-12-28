/**
 * Copyright (c) 2023 Kevin J. Walters
 *
 * SPDX-License-Identifier: MIT
 */


#include "effect/effect.hpp"


void Effect::init(void) { };
void Effect::setSampleRate(uint32_t) { };
void Effect::update(int16_t *buffer16, size_t sample_count, uint16_t channels) { };
void Effect::update(uint16_t *buffer16, size_t sample_count, uint16_t channels) { };
void Effect::start(void) { };
void Effect::stop(void) { };
void Effect::buttonB(void) { };
void Effect::buttonC(void) { };

