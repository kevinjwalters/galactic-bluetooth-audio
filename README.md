# Unicorn Audio Display

Unicorn Audio Display is firmware for the Raspberry Pi Pico W-powered Galactic Unicorn and Cosmic Unicorn from Pimoroni based on Pimoroni's Blunicorn firmware. It acts as a Bluetooth A2DP Speaker or reads the analogue signal on GP27 for use with microphone modules.

Play music to your display, and bask in the warm, LED glow of real-time visualisation effects.

## Installing

You should grab the latest release from https://github.com/kevinjwalters/unicorn-audio-display/releases/latest

Make sure to grab the right package for your board.

Connect your board to a computer via USB.

Reset your board by holding the BOOTSEL (the button on the Pico itself) button and pressing RESET.

Extract the firmware (.uf2 file) and drag it over to the `RPI-RP2` drive.

## Usage

Fire up Bluetooth on your phone or PC, you should see a new "Cosmic Unicorn" or "Galactic Unicorn" device. Connect and play music to see pretty, pretty colours!

Use button A to switch effect.

## Building

For Galactic Unicorn:

```bash
mkdir build.cosmic
cd build.cosmic
cmake .. -DPICO_SDK_PATH=${PICO_SDK_PATH} -DPICO_EXTRAS_PATH=${PICO_EXTRAS_PATH} -DPICO_BOARD=pico_w -DUNICORN_MODEL=galactic -DCMAKE_BUILD_TYPE=Release
```

For Cosmic Unicorn:

```bash
mkdir build.cosmic
cd build.cosmic
cmake .. -DPICO_SDK_PATH=${PICO_SDK_PATH} -DPICO_EXTRAS_PATH=${PICO_EXTRAS_PATH} -DPICO_BOARD=pico_w -DUNICORN_MODEL=cosmic -DCMAKE_BUILD_TYPE=Release
```
