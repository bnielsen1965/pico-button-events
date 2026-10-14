// pico-button-events.h - Debounced button state machine producing
// click/hold/release events. Port of the JavaScript "button-events" module
// (github.com/bnielsen1965/button-events): the EventEmitter is replaced by a
// simple callback, and setTimeout by absolute_time_t deadlines evaluated in
// buttonTickAll().
//
// Works on any Pico platform: RP2040 and RP2350, Cortex-M or Hazard3 RISC-V
// cores, bare-metal (single or dual core) or RTOS.

#ifndef PICO_BUTTON_EVENTS_H
#define PICO_BUTTON_EVENTS_H

#include <stdbool.h>
#include <stdint.h>

#include "pico/time.h"

// Maximum number of buttons managed by the module
#define BUTTON_MAX 8

// Timing windows in milliseconds (mirrors the JS module's Config.timing:
// debounce / pressed / clicked). Defaults are tuned for fast, natural
// tapping: a tap held under ~150 ms is a click, holding past ~150 ms is
// a long press, and a new tap within 150 ms of a release chains into a
// double/triple/quadruple click.
typedef struct
{
    uint32_t debounce_ms; // milliseconds to debounce the input
    uint32_t pressed_ms;  // hold time that becomes a "pressed" (long press)
    uint32_t clicked_ms;  // window in which a new tap chains into a multi-click
} button_timing_t;

// Get / set the timing windows (applies to all buttons; may be changed at
// runtime). Must be called from the core that calls buttonTickAll().
void buttonGetTiming(button_timing_t *timing);
void buttonSetTiming(const button_timing_t *timing);

// Button event types (mirrors the button-events event names)
typedef enum {
    BUTTON_EVENT_PRESSED = 0,          // held for the "pressed" window
    BUTTON_EVENT_CLICKED,              // released within the press window
    BUTTON_EVENT_CLICKED_PRESSED,      // clicked, then pressed again (held)
    BUTTON_EVENT_DOUBLE_CLICKED,
    BUTTON_EVENT_DOUBLE_CLICKED_PRESSED,
    BUTTON_EVENT_TRIPLE_CLICKED,
    BUTTON_EVENT_TRIPLE_CLICKED_PRESSED,
    BUTTON_EVENT_QUADRUPLE_CLICKED,    // fourth press emits immediately
    BUTTON_EVENT_RELEASED,             // released after a long press
} button_event_t;

// Callback invoked (on the core that calls buttonTickAll) for each event
typedef void (*button_event_cb)(uint button_index, button_event_t event);

// Configure a button: GPIO as input with internal pull-up (button pulls the
// GPIO to ground when pressed), and falling+rising edge IRQs whose ISR only
// records that an edge occurred. Must be called on the core that will run
// buttonTickAll (SIO GPIO IRQs are per-core).
void buttonInit(uint button_index, uint gpio, button_event_cb callback);

// Run the debounce + state machine and emit pending events. Call
// periodically (roughly every 20 ms) from one core.
void buttonTickAll(absolute_time_t now);

// Human-readable name of an event, for logging
const char *buttonEventName(button_event_t event);

#endif // PICO_BUTTON_EVENTS_H
