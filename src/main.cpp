/**
 * Copyright (c) 2023 Pimoroni Ltd <phil@pimoroni.com>
 * Copyright (c) 2023 Kevin J. Walters
 *
 * SPDX-License-Identifier: MIT
 */


#include <cstdlib>
#include <memory>
#include <vector>

#include "pico/stdlib.h"

#include "uad/unicornaudiodisplay.hpp"
#include "uad/audiosource.hpp"
#include "hardware/vreg.h"

static constexpr int EXIT_INIT_FAILED = 11;

uint32_t debug = 1;


int main(int argc, char *argv[]) {
    // TODO ponder Pimoroni's old clock to 200MHz with voltage bump
    //vreg_set_voltage(VREG_VOLTAGE_1_20);
    //sleep_ms(10);
    //set_sys_clock_khz(200000, true);

    stdio_init_all();

    UnicornAudioDisplay UAD;
#ifdef AUDIO_SOURCES
    std::vector<std::shared_ptr<AudioSource>> sources {std::make_shared<AUDIO_SOURCES>(&UAD)};
#else
    std::vector<std::shared_ptr<AudioSource>> sources {std::make_shared<AudioSourceDMAADC>(&UAD)};
    //std::vector<std::shared_ptr<AudioSource>> sources {std::make_shared<AudioSourceBluetooth>(&UAD)};
#endif
    UAD.init(sources);
    if (!UAD.okay()) {
        exit(EXIT_INIT_FAILED);
    }

    UAD.run();  // At the moment this never returns
    UAD.deinit();
}

