/**
 * Copyright (c) 2023 Kevin J. Walters
 *
 * SPDX-License-Identifier: MIT
 */


#pragma once

#include <cstdint>


class UnicornAudioDisplay;  // For pointers


class AudioSource {
  protected:
    bool active;
    bool init_okay;
    UnicornAudioDisplay *uad_ptr;

  public:
    explicit AudioSource(UnicornAudioDisplay *uad_ptr_) :
        active(false),
        init_okay(true),
        uad_ptr(uad_ptr_) { };
    virtual ~AudioSource() {};

    virtual void init(void) = 0;
    virtual void run(void) = 0;
    virtual void deinit(void) = 0;
    virtual bool okay(void) = 0;
    virtual uint32_t getSampleRate(void) = 0;
};

class AudioSourceDMAADC : public AudioSource {
  public:
    explicit AudioSourceDMAADC(UnicornAudioDisplay *uad_ptr_) : AudioSource(uad_ptr_),
        collected_adc1_counter(0),
        collected_adc2_counter(0) { };
    void init(void) override;
    void run(void) override;
    void deinit(void) override;
    bool okay(void) override;
    virtual uint32_t getSampleRate(void) { return 96000; };  // TODO - proper implementation!!

  protected:
    int collected_adc1_counter;
    int collected_adc2_counter;
};

class AudioSourceBluetooth : public AudioSource {
  public:
    explicit AudioSourceBluetooth(UnicornAudioDisplay *uad_ptr_) : AudioSource(uad_ptr_) { };
    void init(void) override;
    void run(void) override;
    void deinit(void) override;
    bool okay(void) override;
    virtual uint32_t getSampleRate(void) { return 44100; };  // TODO - proper implementation!! including the magic to set it when value appears ie return 0 here
};

