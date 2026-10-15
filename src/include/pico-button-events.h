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

// Maximum number of simultaneous chord groups (see buttonChordInit)
#define BUTTON_CHORD_MAX 4

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
    uint32_t chord_ms;    // settle window for press-mode chords (see below)
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

// ---- Chord detection -------------------------------------------------------
// A chord group is a set of consecutive buttons treated as one unit: the
// buttons held down together form a single chord, represented as a bitmask
// (bit i = the (first_index + i)-th button in the group). For a group of 4
// buttons the mask runs 0x1..0xF: a single tap is 0x1/0x2/0x4/0x8, two
// buttons held together are 0x3/0x5/0x6/..., and all four are 0xF.
//
// When the chord fires depends on the mode (button_chord_mode_t):
//   BUTTON_CHORD_MODE_RELEASE - when the group goes from "some down" back to
//     "none down", i.e. when the last button is released.
//   BUTTON_CHORD_MODE_PRESS   - once the pressed buttons have been stable for
//     the chord_ms settle window; the mask is the buttons currently down.
//
// The callback is invoked (on the core that calls buttonTickAll) with the
// mask. The group's buttons must be configured with buttonInit() first; a
// NULL per-button callback is fine if you only want chord events.
typedef void (*button_chord_cb)(uint mask);

// When a chord group fires (see buttonChordInit)
typedef enum
{
    BUTTON_CHORD_MODE_RELEASE = 0, // on the last button's release
    BUTTON_CHORD_MODE_PRESS,       // once the pressed set is stable for chord_ms
} button_chord_mode_t;

void buttonChordInit(uint first_index, uint count, button_chord_cb callback,
                     button_chord_mode_t mode);

// Run the debounce + state machine and emit pending events. Call
// periodically (roughly every 20 ms) from one core.
void buttonTickAll(absolute_time_t now);

// Human-readable name of an event, for logging
const char *buttonEventName(button_event_t event);

#endif // PICO_BUTTON_EVENTS_H
