add_library(console_recorder INTERFACE)

target_sources(console_recorder INTERFACE
  ${CMAKE_CURRENT_LIST_DIR}/console_recorder.cpp
  ${CMAKE_CURRENT_LIST_DIR}/effect_recorder.cpp
  ${CMAKE_CURRENT_LIST_DIR}/lib/recorder.cpp
)

target_include_directories(console_recorder INTERFACE
  ${CMAKE_CURRENT_LIST_DIR}
)

