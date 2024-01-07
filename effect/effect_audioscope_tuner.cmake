add_library(effect_audioscope_tuner INTERFACE)

target_sources(classic_tuner INTERFACE
  ${CMAKE_CURRENT_LIST_DIR}/effect_audioscope_tuner.cpp
  ${CMAKE_CURRENT_LIST_DIR}/effect_tuner.cpp
  ${CMAKE_CURRENT_LIST_DIR}/lib/tuner.cpp
)

target_include_directories(effect_audioscope_tuner INTERFACE
  ${CMAKE_CURRENT_LIST_DIR}
)

