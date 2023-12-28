add_library(unicornaudiodisplay INTERFACE)

if(UNICORN_MODEL STREQUAL galactic)
  add_definitions(-DUNICORN_MODEL_NUM=1)
elseif(UNICORN_MODEL STREQUAL cosmic)
  add_definitions(-DUNICORN_MODEL_NUM=2)
elseif(UNICORN_MODEL STREQUAL stellar)
  add_definitions(-DUNICORN_MODEL_NUM=3)
else()
  message(ERROR UNICORN_MODEL not recognised)
endif()

target_sources(unicornaudiodisplay INTERFACE
  ${CMAKE_CURRENT_LIST_DIR}/audiosourcedmaadc.cpp
  ${CMAKE_CURRENT_LIST_DIR}/audiosourcebluetooth.cpp
)

target_include_directories(unicornaudiodisplay INTERFACE
  ${CMAKE_CURRENT_LIST_DIR}
)
