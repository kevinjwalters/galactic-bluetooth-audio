/**
 * Copyright (C) 2022 BlueKitchen GmbH
 * Copyright (c) 2023 Kevin J Walters
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <cstddef>

#include "btstack_audio.h"
#include "btstack_debug.h"
#include "btstack_event.h"
#include "btstack_run_loop.h"

#include "pico/audio_i2s.h"
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "pico/time.h"
#include "boards/pico.h"
#include "hardware/dma.h"

// #include "pico/multicore.h"
// #include "pico/sync.h"

#include "uad/audiosource.hpp"
#include "uad/debug.hpp"
#include "uad/display.hpp"
#include "uad/unicornaudiodisplay.hpp"

#include "effect/lib/fixed_fft.hpp"
#include "effect/effect.hpp"


// Hack for setting the volume as no context is passed to callback
// this is initialised by init()
static UnicornAudioDisplay *global_uad_ptr = nullptr;

static constexpr uint32_t DRIVER_POLL_INTERVAL_MS = 5;


static constexpr uint16_t MONO = 1;
static constexpr uint16_t STEREO_CHANNELS = 2;
static constexpr unsigned int SAMPLES_PER_AUDIO_CHANNEL = 512;
static constexpr unsigned int SAMPLES_AUDIO_CHANNELS = SAMPLES_PER_AUDIO_CHANNEL * STEREO_CHANNELS;

// client
static void (*playback_callback)(int16_t *buffer, uint16_t num_samples);

// timer to fill output ring buffer
static btstack_timer_source_t  driver_timer_sink;

static bool btstack_audio_pico_sink_active = false;

// from pico-playground/audio/sine_wave/sine_wave.c

static audio_format_t        btstack_audio_pico_audio_format;
static audio_buffer_format_t btstack_audio_pico_producer_format;
static audio_buffer_pool_t * btstack_audio_pico_audio_buffer_pool;
static uint8_t               btstack_audio_pico_channel_count;


int64_t effect_update_time[16] = {0};
int eu_idx = 0;

static int buf_no = 0;  // for debug


static audio_buffer_pool_t *init_audio(uint32_t sample_frequency, uint8_t channel_count) {
    // num channels requested by application
    btstack_audio_pico_channel_count = channel_count;

    printf("init_audio 1\n");
    // This is the output format for i2s and is passed to pico-extras audio_i2s_setup
    // always use stereo
    btstack_audio_pico_audio_format.format = AUDIO_BUFFER_FORMAT_PCM_S16;
    btstack_audio_pico_audio_format.sample_freq = sample_frequency;
    btstack_audio_pico_audio_format.channel_count = STEREO_CHANNELS;

    btstack_audio_pico_producer_format.format = &btstack_audio_pico_audio_format;
    btstack_audio_pico_producer_format.sample_stride = sizeof(int16_t) * STEREO_CHANNELS;

    // 3 is channel_count and comes from https://github.com/raspberrypi/pico-examples/blob/master/pico_w/bt/btstack_audio_pico.c
    // which has the mysterious comment of  // todo correct size
    audio_buffer_pool_t * producer_pool = audio_new_producer_pool(&btstack_audio_pico_producer_format,
                                                                  3, SAMPLES_PER_AUDIO_CHANNEL);

    audio_i2s_config_t config;
    config.data_pin       = PICO_AUDIO_I2S_DATA_PIN;
    config.clock_pin_base = PICO_AUDIO_I2S_CLOCK_PIN_BASE;
    config.dma_channel    = (int8_t) dma_claim_unused_channel(true);
    config.pio_sm         = 0;
    printf("init_audio 2\n");
    sleep_ms(100);

    // audio_i2s_setup claims the channel again https://github.com/raspberrypi/pico-extras/issues/48
    dma_channel_unclaim(config.dma_channel);
    printf("init_audio 3\n");
    sleep_ms(100);
    const audio_format_t * output_format = audio_i2s_setup(&btstack_audio_pico_audio_format, &config);
    if (!output_format) {
        panic("PicoAudio: Unable to open audio device.\n");
    }
    printf("init_audio 4\n");
    sleep_ms(100);

    bool ok = audio_i2s_connect(producer_pool);
    assert(ok);
    (void)ok;
    printf("init_audio 5\n");

    return producer_pool;
}


// DOC This is based on https://github.com/raspberrypi/pico-examples/blob/master/pico_w/bt/btstack_audio_pico.c
static void btstack_audio_pico_sink_fill_buffers(UnicornAudioDisplay *uad_ptr) {
    int loops = 0;
    while (true) {
        audio_buffer_t *audio_buffer = take_audio_buffer(btstack_audio_pico_audio_buffer_pool, false);
        if (audio_buffer == NULL) {
            break;
        }
        ++loops;

        int16_t *buffer16 = reinterpret_cast<int16_t *>(audio_buffer->buffer->bytes);

        // TODO - check what this is doing and ponder max vs actual amount of data
        // TODO - order of playback vs buffer modifications, is there a race condition here?
        (*playback_callback)(buffer16, audio_buffer->max_sample_count);

        uad_ptr->processData(buffer16, audio_buffer->max_sample_count, STEREO_CHANNELS);

        for (auto i = 0u; i < SAMPLES_AUDIO_CHANNELS; i++) {
            // TODO - btstack_volume is needed here
            buffer16[i] = (int32_t(buffer16[i]) * int32_t(uad_ptr->local_volume)) >> 7;
        }

        // duplicate samples for mono
        if (btstack_audio_pico_channel_count == 1) {
            for (auto i = audio_buffer->max_sample_count - 1 ; i >= 0; i--) {
                buffer16[2*i  ] = buffer16[i];
                buffer16[2*i+1] = buffer16[i];
            }
        }
        audio_buffer->sample_count = audio_buffer->max_sample_count;
        give_audio_buffer(btstack_audio_pico_audio_buffer_pool, audio_buffer);
        buf_no++;
    }
    if (loops > 4) {
        printf("WARNING loops %d is greather than 4\n", loops);
    }
}

static void driver_timer_handler_sink(btstack_timer_source_t *ts) {
    UnicornAudioDisplay *uad_ptr = reinterpret_cast<UnicornAudioDisplay*>(ts->context);

    uad_ptr->checkButtons();

    // refill
    btstack_audio_pico_sink_fill_buffers(uad_ptr);

    // re-set timer
    btstack_run_loop_set_timer(ts, DRIVER_POLL_INTERVAL_MS);
    btstack_run_loop_add_timer(ts);
}

static int btstack_audio_pico_sink_init(
    uint8_t channels,
    uint32_t samplerate,
    void (*playback)(int16_t * buffer, uint16_t num_samples)) {
    btstack_assert(playback != NULL);
    btstack_assert(channels != 0);

    // This is probably enabled audio output on the Unicorn
    gpio_init(Display::MUTE);
    gpio_set_dir(Display::MUTE, GPIO_OUT);
    gpio_put(Display::MUTE, true);

    playback_callback  = playback;
    global_uad_ptr->setSampleRate(samplerate);
    btstack_audio_pico_audio_buffer_pool = init_audio(samplerate, channels);

    return 0;
}

static void btstack_audio_pico_sink_set_volume(uint8_t volume) {
    global_uad_ptr->local_volume = volume;
}

static void btstack_audio_pico_sink_start_stream(void) {
    // pre-fill HAL buffers
    btstack_audio_pico_sink_fill_buffers(global_uad_ptr);

    // start timer
    btstack_run_loop_set_timer_handler(&driver_timer_sink, &driver_timer_handler_sink);
    btstack_run_loop_set_timer_context(&driver_timer_sink, global_uad_ptr);
    btstack_run_loop_set_timer(&driver_timer_sink, DRIVER_POLL_INTERVAL_MS);
    btstack_run_loop_add_timer(&driver_timer_sink);
    // state
    btstack_audio_pico_sink_active = true;

    audio_i2s_set_enabled(true);
}

static void btstack_audio_pico_sink_stop_stream(void) {
    audio_i2s_set_enabled(false);

    // stop timer
    btstack_run_loop_remove_timer(&driver_timer_sink);
    // state
    btstack_audio_pico_sink_active = false;
}

static void btstack_audio_pico_sink_close(void) {
    // stop stream if needed
    if (btstack_audio_pico_sink_active) {
        btstack_audio_pico_sink_stop_stream();
    }
}

static const btstack_audio_sink_t btstack_audio_pico_sink = {
    .init = &btstack_audio_pico_sink_init,
    .set_volume = &btstack_audio_pico_sink_set_volume,
    .start_stream = &btstack_audio_pico_sink_start_stream,
    .stop_stream = &btstack_audio_pico_sink_stop_stream,
    .close = &btstack_audio_pico_sink_close,
};


int btstack_main(int argc, const char * argv[]);

static btstack_packet_callback_registration_t hci_event_callback_registration;

static void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size) {
    UNUSED(size);
    UNUSED(channel);
    bd_addr_t local_addr;
    if (packet_type != HCI_EVENT_PACKET) return;
    switch (hci_event_packet_get_type(packet)) {
        case BTSTACK_EVENT_STATE:
            if (btstack_event_state_get_state(packet) != HCI_STATE_WORKING) return;
            gap_local_bd_addr(local_addr);
            if (debug >=1) {
                printf("INFO: BTstack up and running on %s.\n", bd_addr_to_str(local_addr));
            }
            break;
        default:
            break;
    }
}


void AudioSourceBluetooth::init(void) {
    global_uad_ptr = uad_ptr;
    // initialize CYW43 driver architecture (will enable BT if/because CYW43_ENABLE_BLUETOOTH == 1)
    if (cyw43_arch_init()) {
        // I think this returns 0 BUT later fails WITH DMA ADC
        printf("FATAL: failed to initialise cyw43_arch\n");
        init_okay = false;
        return;
    }

    // inform about BTstack state
    hci_event_callback_registration.callback = &packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    // setup i2s audio for sink
    btstack_audio_sink_set_instance(&btstack_audio_pico_sink);
}

void AudioSourceBluetooth::run(void) {
    btstack_main(0, NULL);
    btstack_run_loop_execute();
}

void AudioSourceBluetooth::deinit(void) {
    // Nothing to do at the moment
}

bool AudioSourceBluetooth::okay(void) {
    return init_okay;
}

