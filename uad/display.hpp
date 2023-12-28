/**
 * Copyright (c) 2023 Kevin J. Walters
 *
 * SPDX-License-Identifier: MIT
 */


#pragma once

#ifdef UNICORN_MODEL_NUM

#if UNICORN_MODEL_NUM == 1
#include "galactic_unicorn.hpp"
using Display = pimoroni::GalacticUnicorn;
#elif UNICORN_MODEL_NUM == 2
#include "cosmic_unicorn.hpp"
using Display =  pimoroni::CosmicUnicorn;
#elif UNICORN_MODEL_NUM == 3
#include "stellar_unicorn.hpp"
using Display =  pimoroni::StellarUnicorn;
#else
#error Unknown UNICORN_MODEL_NUM
#endif

#else
#error UNICORN_MODEL_NUM not defined
#endif
