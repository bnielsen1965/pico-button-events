# pico-button-events — debounced button events for Raspberry Pi Pico
# (RP2040 and RP2350, Cortex-M or Hazard3 RISC-V cores).
#
# INTERFACE library: consumers add this directory and link `pico-button-events`.
# The only SDK dependencies are pico_time (absolute_time_t) and
# hardware_gpio (pin setup + edge IRQs), so the module is portable across
# every Pico platform.
add_library(pico-button-events INTERFACE)

target_sources(pico-button-events INTERFACE
  ${CMAKE_CURRENT_LIST_DIR}/src/pico-button-events.c
)

target_include_directories(pico-button-events INTERFACE
  ${CMAKE_CURRENT_LIST_DIR}/src/include
)

target_link_libraries(pico-button-events INTERFACE
  pico_time
  hardware_gpio
)
