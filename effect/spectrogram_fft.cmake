add_library(spectrogram_fft INTERFACE)

target_sources(spectrogram_fft INTERFACE
  ${CMAKE_CURRENT_LIST_DIR}/spectrogram_fft.cpp
  ${CMAKE_CURRENT_LIST_DIR}/effect_fft.cpp
  ${CMAKE_CURRENT_LIST_DIR}/lib/fixed_fft.cpp
)

target_include_directories(spectrogram_fft INTERFACE
  ${CMAKE_CURRENT_LIST_DIR}
)

# Choose one:
# SCALE_LOGARITHMIC
# SCALE_SQRT
# SCALE_LINEAR
target_compile_definitions(spectrogram_fft INTERFACE
  -DSCALE_LOGARITHMIC
)
