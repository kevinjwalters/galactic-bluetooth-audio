add_library(effect INTERFACE)

target_sources(effect INTERFACE
  ${CMAKE_CURRENT_LIST_DIR}/effect.cpp
)

target_include_directories(effect INTERFACE
  ${CMAKE_CURRENT_LIST_DIR}
)

