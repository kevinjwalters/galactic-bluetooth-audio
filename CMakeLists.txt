cmake_minimum_required(VERSION 3.12)

set(NAME1 unicornaudiodisplay-bluetooth)
set(NAME2 unicornaudiodisplay-dmaadc)

include(pico_sdk_import.cmake)
include(pico_extras_import.cmake)

project(${NAME1} C CXX ASM)
project(${NAME2} C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

include_directories(
  ${CMAKE_CURRENT_LIST_DIR}
)

include(uad/unicornaudiodisplay.cmake)
include(bluetooth/bluetooth.cmake)
include(effect/effect.cmake)
include(effect/spectrogram_fft.cmake)
include(effect/rainbow_fft.cmake)
include(effect/classic_fft.cmake)
include(effect/classic_tuner.cmake)
include(effect/console_recorder.cmake)

include(libraries/bitmap_fonts/bitmap_fonts.cmake)
include(libraries/hershey_fonts/hershey_fonts.cmake)
include(libraries/pico_graphics/pico_graphics.cmake)

if(UNICORN_MODEL AND EXISTS ${CMAKE_CURRENT_LIST_DIR}/libraries/${UNICORN_MODEL}_unicorn/${UNICORN_MODEL}_unicorn.cmake)
else()
message(WARNING "Using default display for Galactic Unicorn...")
set(UNICORN_MODEL galactic)
endif()
include(libraries/${UNICORN_MODEL}_unicorn/${UNICORN_MODEL}_unicorn.cmake)

# if(DISPLAY_PATH AND EXISTS ${CMAKE_CURRENT_LIST_DIR}/${DISPLAY_PATH})
# include(${CMAKE_CURRENT_LIST_DIR}/${DISPLAY_PATH})
# else()
# message(WARNING "Using default display (display/galactic/galactic_unicorn.cmake)...")
# include(display/galactic/galactic_unicorn.cmake)
# endif()

add_executable(${NAME1}
    ${CMAKE_CURRENT_LIST_DIR}/src/main.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/a2dp_sink.cpp
)

add_executable(${NAME2}
    ${CMAKE_CURRENT_LIST_DIR}/src/main.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/a2dp_sink.cpp
)

pico_set_program_name(${NAME1} "unicornaudiodisplay")
pico_set_program_version(${NAME1} "1.3.0")
pico_set_program_url(${NAME1} "https://www.instructables.com/member/kevinjwalters/")
pico_set_program_name(${NAME2} "unicornaudiodisplay")
pico_set_program_version(${NAME2} "1.3.0")
pico_set_program_url(${NAME2} "https://www.instructables.com/member/kevinjwalters/")

target_include_directories(${NAME1} PRIVATE
#    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/effect
    ${CMAKE_CURRENT_LIST_DIR}/libraries/pico_graphics
    ${CMAKE_CURRENT_LIST_DIR}/libraries/${UNICORN_MODEL}_unicorn
)
target_include_directories(${NAME2} PRIVATE
#    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/effect
    ${CMAKE_CURRENT_LIST_DIR}/libraries/pico_graphics
    ${CMAKE_CURRENT_LIST_DIR}/libraries/${UNICORN_MODEL}_unicorn
)

target_link_libraries(${NAME1}
    picow_bt_unicorn_audio
    pico_audio_i2s
    pico_multicore
    ${UNICORN_MODEL}_unicorn
    effect
    spectrogram_fft
    rainbow_fft
    classic_fft
    classic_tuner
    console_recorder
    unicornaudiodisplay
)
target_link_libraries(${NAME2}
    picow_bt_unicorn_audio
    pico_audio_i2s
    pico_multicore
    ${UNICORN_MODEL}_unicorn
    effect
    spectrogram_fft
    rainbow_fft
    classic_fft
    classic_tuner
    console_recorder
    unicornaudiodisplay

    hardware_adc
    hardware_dma
)

# Create the Bluetooth device name with upper case first character
string(SUBSTRING ${UNICORN_MODEL} 0 1 FIRST_LETTER)
string(TOUPPER ${FIRST_LETTER} FIRST_LETTER)
string(REGEX REPLACE "^.(.*)" "${FIRST_LETTER}\\1 Unicorn" BLUETOOTH_DEVICE_NAME "${UNICORN_MODEL}")

message(NOTICE "Bluetooth device name: ${BLUETOOTH_DEVICE_NAME}")

target_compile_definitions(${NAME1} PRIVATE
    PICO_AUDIO_I2S_DATA_PIN=9
    PICO_AUDIO_I2S_CLOCK_PIN_BASE=10
    BLUETOOTH_DEVICE_NAME="${BLUETOOTH_DEVICE_NAME}"
    AUDIO_SOURCES=AudioSourceBluetooth
)
target_compile_definitions(${NAME2} PRIVATE
    PICO_AUDIO_I2S_DATA_PIN=9
    PICO_AUDIO_I2S_CLOCK_PIN_BASE=10
    BLUETOOTH_DEVICE_NAME="${BLUETOOTH_DEVICE_NAME}"
    AUDIO_SOURCES=AudioSourceDMAADC
)

pico_enable_stdio_usb(${NAME1} 1)
pico_enable_stdio_usb(${NAME2} 1)
pico_add_extra_outputs(${NAME1})
pico_add_extra_outputs(${NAME2})

# Set up files for the release packages
install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/${NAME1}.uf2
    ${CMAKE_CURRENT_BINARY_DIR}/${NAME2}.uf2
    ${CMAKE_CURRENT_LIST_DIR}/README.md
    DESTINATION .
)

set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY OFF)
set(CPACK_GENERATOR "ZIP" "TGZ")
include(CPack)
