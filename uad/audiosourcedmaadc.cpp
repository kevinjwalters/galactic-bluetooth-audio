/**
 * Copyright (c) 2023 Kevin J. Walters
 *
 * SPDX-License-Identifier: MIT
 */


#include "pico/stdlib.h"
#include "pico/time.h"
#include "pico/cyw43_arch.h"
#include "pico/btstack_run_loop_async_context.h"
#include "hardware/adc.h"
#include "hardware/dma.h"
#include "hardware/irq.h"

#include "btstack_event.h"
#include "btstack_memory.h"
#include "btstack_run_loop.h"
// See pico/btstack_run_loop_async_context.h above

#include "pico_graphics.hpp"

#include "uad/audiosource.hpp"
#include "uad/display.hpp"
#include "uad/debug.hpp"
#include "uad/unicornaudiodisplay.hpp"

#include "lib/fixed_fft.hpp"
#include "effect.hpp"


typedef uint16_t adc_uint;

int btstack_main(int argc, const char * argv[]);

static constexpr int ADC_BASE_PIN = 26;  // TODO - get this from somewhere

static constexpr uint32_t DRIVER_POLL_INTERVAL_MS = 1;

static btstack_timer_source_t  driver_timer_sink;

static constexpr uint16_t MONO = 1;
static constexpr uint16_t STEREO_CHANNELS = 2;
static constexpr unsigned int SAMPLES_PER_AUDIO_CHANNEL = 512;
static constexpr unsigned int SAMPLES_AUDIO_CHANNELS = SAMPLES_PER_AUDIO_CHANNEL * STEREO_CHANNELS;

static constexpr uint64_t ADC_CLOCK = 48'000'000;  // 48 MHz
// 44100 isn't ideal here as it doesn't divide the clock exactly
// higher frequencies are attractive for the Tuner as it gives improved
// accuracy at high frequencies with penalty of raised minimum frequency
static constexpr uint ADC_SAMPLE_RATE = 96000;

static constexpr uint DMA_RING_BITS = 12u;  // 4096 bytes for 2k samples
static constexpr uint ADC_SAMPLE_BUFFER_SIZE = 1u << DMA_RING_BITS;
typedef uint16_t adc_uint;
static constexpr uint ADC_SAMPLE_BUFFER_LEN = ADC_SAMPLE_BUFFER_SIZE / sizeof(adc_uint);
// TODO consider setting TRANSFER_LOW based on the sample rate
static constexpr uint TRANSFER_LOW = 600;  // 6.25ms at 96k
static constexpr uint TRANSFER_HIGH = uint(ADC_SAMPLE_BUFFER_LEN - 10);

// These need to be "naturally-aligned" in memory for DMA
// to work correctly
static adc_uint adc_buf1[ADC_SAMPLE_BUFFER_LEN] __attribute__((aligned(ADC_SAMPLE_BUFFER_SIZE))) = {0};
static adc_uint adc_buf2[ADC_SAMPLE_BUFFER_LEN] __attribute__((aligned(ADC_SAMPLE_BUFFER_SIZE))) = {0};

uint adc_dma_chan1 = 666u;
uint adc_dma_chan2 = 666u;

static volatile int adc_buf1_counter = 0;
static volatile int adc_buf2_counter = 0;
static volatile int other_counter = 0;

static int collected_adc1_counter = 0;
static int collected_adc2_counter = 0;


static void __isr dma_irq0_handler() {
    // Increment the relevant counter and clear interrupt
    if (dma_channel_get_irq0_status(adc_dma_chan1)) {
        adc_buf1_counter = adc_buf1_counter + 1;
        dma_channel_acknowledge_irq0(adc_dma_chan1);
    } else if (dma_channel_get_irq0_status(adc_dma_chan2)) {
        adc_buf2_counter = adc_buf2_counter + 1;
        dma_channel_acknowledge_irq0(adc_dma_chan2);
    } else {
        other_counter = other_counter + 1;
    }
}

static void __isr dma_irq1_handler() {
    // Increment the relevant counter and clear interrupt
    if (dma_channel_get_irq1_status(adc_dma_chan1)) {
        adc_buf1_counter = adc_buf1_counter + 1;
        dma_channel_acknowledge_irq1(adc_dma_chan1);
    } else if (dma_channel_get_irq1_status(adc_dma_chan2)) {
        adc_buf2_counter = adc_buf2_counter + 1;
        dma_channel_acknowledge_irq1(adc_dma_chan2);
    } else {
        other_counter = other_counter + 1;
    }
}


static void init_adc(void) {
    if (debug >=1) {
        printf("init_adc begin\n");
    }

    //adc_gpio_init(Display::LIGHT_SENSOR);
    adc_gpio_init(Display::SWITCH_SLEEP);
    adc_init();
    // Can test against Display::LIGHT_SENSOR on Unicorns too
    uint adc_channel = Display::SWITCH_SLEEP - ADC_BASE_PIN;
    adc_select_input(adc_channel);

    adc_fifo_setup(
        true,    // Enables write each conversion result to the FIFO
        true,    // Enable DMA requests when FIFO contains data
        1,       // Threshold for DMA requests/FIFO IRQ if enabled
        false,    // If enabled, bit 15 of the FIFO contains error flag for each sample
        false    // Shift FIFO contents to be one byte in size (for byte DMA)
    );

    // 48MHz clock
    adc_set_clkdiv(ADC_CLOCK / ADC_SAMPLE_RATE - 1);

    adc_dma_chan1 = dma_claim_unused_channel(true);
    adc_dma_chan2 = dma_claim_unused_channel(true);

    dma_channel_config dma_config1 = dma_channel_get_default_config(adc_dma_chan1);
    channel_config_set_transfer_data_size(&dma_config1, DMA_SIZE_16);
    channel_config_set_read_increment(&dma_config1, false);
    channel_config_set_write_increment(&dma_config1, true);
    channel_config_set_ring(&dma_config1, true, DMA_RING_BITS);
    channel_config_set_dreq(&dma_config1, DREQ_ADC);
    // channel_config_set_irq_quiet(&dma_config1, true);
    channel_config_set_chain_to(&dma_config1, adc_dma_chan2); // ping-pong
    dma_channel_configure(adc_dma_chan1,
                          &dma_config1,
                          adc_buf1,       // dst
                          &adc_hw->fifo,  // src
                          ADC_SAMPLE_BUFFER_LEN,  // adc_buf1 length
                          false);         // do not start immediately

    dma_channel_config dma_config2 = dma_channel_get_default_config(adc_dma_chan2);
    channel_config_set_transfer_data_size(&dma_config2, DMA_SIZE_16);
    channel_config_set_read_increment(&dma_config2, false);
    channel_config_set_write_increment(&dma_config2, true);
    channel_config_set_ring(&dma_config2, true, DMA_RING_BITS);
    channel_config_set_dreq(&dma_config2, DREQ_ADC);
    channel_config_set_chain_to(&dma_config2, adc_dma_chan1); // ping-pong
    // channel_config_set_irq_quiet(&dma_config2, true);
    dma_channel_configure(adc_dma_chan2,
                          &dma_config2,
                          adc_buf2,       // dst
                          &adc_hw->fifo,  // src
                          ADC_SAMPLE_BUFFER_LEN,  // adc_buf2 length
                          false);         // do not start immediately

    irq_set_exclusive_handler(DMA_IRQ_0, dma_irq0_handler);
    // irq_set_exclusive_handler(DMA_IRQ_1, dma_irq1_handler);
    dma_channel_set_irq0_enabled(adc_dma_chan1, true);
    dma_channel_set_irq0_enabled(adc_dma_chan2, true);
    //dma_channel_set_irq1_enabled(adc_dma_chan1, true);
    //dma_channel_set_irq1_enabled(adc_dma_chan2, true);
    irq_set_enabled(DMA_IRQ_0, true);
    // irq_set_enabled(DMA_IRQ_1, true);

    adc_run(true);
    // These do something with both unchained and both started. Odd!
    dma_channel_start(adc_dma_chan1);
    //dma_channel_start(adc_dma_chan2);

    // A slight pause to ensure printf breaks if DMA is "misbehaving"
    sleep_ms(10);
    if (debug >=1) {
        printf("init_adc end\n");
    }
}

// Using suggestion from https://forums.raspberrypi.com/viewtopic.php?p=2024083
// which is based on dma_channel_abort() code
void dma_channel_abort2(uint chan1, uint chan2) {
    // Atomically abort both channels.
    dma_hw->abort = (1 << chan1) | (1 << chan2);

    // Wait for all aborts to complete
    while (dma_hw->abort) { tight_loop_contents(); };

    // Wait for each channel to not be busy.
    while (dma_hw->ch[chan1].ctrl_trig & DMA_CH0_CTRL_TRIG_BUSY_BITS) { tight_loop_contents(); }
    while (dma_hw->ch[chan2].ctrl_trig & DMA_CH0_CTRL_TRIG_BUSY_BITS) { tight_loop_contents(); }
}

static void adc_dma_start(void) {
    dma_channel_start(adc_dma_chan1);
}

static void adc_dma_stop(void) {
    dma_channel_abort2(adc_dma_chan1, adc_dma_chan2);
}



static void adc_timer_handler(btstack_timer_source_t *ts) {
    UnicornAudioDisplay *uad_ptr = reinterpret_cast<UnicornAudioDisplay*>(ts->context);

    uad_ptr->checkButtons();

    // TODO REVIEW THIS
    // Quick hack for now, look for 0 to be transferred and some left in other
    // meaning we have some time
    int before = 0, after = 0, buffer_no = 0, old_count = 0;
    if (adc_buf1_counter > collected_adc1_counter &&
        dma_channel_hw_addr(adc_dma_chan1)->transfer_count == 0 &&
        dma_channel_hw_addr(adc_dma_chan2)->transfer_count > TRANSFER_LOW &&
        dma_channel_hw_addr(adc_dma_chan2)->transfer_count < TRANSFER_HIGH) {
        before = adc_buf1_counter;
        uad_ptr->processData(adc_buf1, ADC_SAMPLE_BUFFER_LEN, MONO);
        after = adc_buf1_counter;
        buffer_no = 1;
        old_count = collected_adc1_counter;
        collected_adc1_counter = before;
    } else if (adc_buf2_counter > collected_adc2_counter &&
        dma_channel_hw_addr(adc_dma_chan2)->transfer_count == 0 &&
        dma_channel_hw_addr(adc_dma_chan1)->transfer_count > TRANSFER_LOW &&
        dma_channel_hw_addr(adc_dma_chan1)->transfer_count < TRANSFER_HIGH) {
        before = adc_buf2_counter;
        uad_ptr->processData(adc_buf2, ADC_SAMPLE_BUFFER_LEN, MONO);
        after = adc_buf2_counter;
        buffer_no = 2;
        old_count = collected_adc2_counter;
        collected_adc2_counter = before;
    }
    if (debug >= 1 && buffer_no >= 1) {
        if (before != after) {
            printf("DMA ADC INFO: buffer %d counter changed (%d,%d)\n",
                   buffer_no, before, after);
        }
        if (before != (old_count + 1)) {
            printf("DMA ADC INFO: buffer %d missed (%d,%d)\n",
                   buffer_no, old_count, before);
        }
    }

    // re-set timer
    btstack_run_loop_set_timer(ts, DRIVER_POLL_INTERVAL_MS);
    btstack_run_loop_add_timer(ts);
}


void AudioSourceDMAADC::init(void) {
    // Docs https://bluekitchen-gmbh.com/btstack/v1.0/how_to/
    // Bits borrowed from cyw43_arch_init() and btstack_cyw43_init()
    async_context_t *a_context = cyw43_arch_async_context();
    if (a_context == NULL) {
        a_context = cyw43_arch_init_default_async_context();
        if (a_context == NULL) {
            return;
        }
        cyw43_arch_set_async_context(a_context);
    }
    btstack_memory_init();
    btstack_run_loop_init(btstack_run_loop_async_context_get_instance(a_context));

    btstack_run_loop_set_timer_handler(&driver_timer_sink, &adc_timer_handler);
    btstack_run_loop_set_timer_context(&driver_timer_sink, uad_ptr);
    btstack_run_loop_set_timer(&driver_timer_sink, DRIVER_POLL_INTERVAL_MS);
    btstack_run_loop_add_timer(&driver_timer_sink);

    init_adc();
    adc_dma_start();
    active = true;
}

uint32_t AudioSourceDMAADC::title(pimoroni::PicoGraphics_PenRGB888 &graphics, int d_width, int d_height) {
    return 0;
}

void AudioSourceDMAADC::run(void) {
    btstack_run_loop_execute();
}

void AudioSourceDMAADC::deinit(void) {
    adc_dma_stop();
    active = false;
}

bool AudioSourceDMAADC::okay(void) {
    return true;
}

