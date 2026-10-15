// pico-button-events.c - Debounced button state machine.
//
// Port of the JavaScript "button-events" module
// (github.com/bnielsen1965/button-events). Correspondence:
//
//   JS                          C
//   ------------------------    ---------------------------------
//   gpioChange(value)           ISR records an edge; buttonTickAll()
//                                re-reads the settled level
//   setTimeout (debounce)       debounceUntil deadline (30 ms)
//   setTimeout (pressed/click)  emitAt deadline (150 ms windows)
//   EventEmitter.emit(name)     callback(button_index, event)
//
// The state machine is driven by buttonTickAll(), which must be called
// periodically (roughly every 20 ms is fine for the 30 / 150 ms timing
// windows) from the core the GPIO interrupts were set up on.
//
// Dual-core contract: the core that calls buttonInit() owns the GPIO IRQs
// (SIO GPIO IRQs are per-core) and must also be the core that calls
// buttonTickAll(). With the ISR and the tick on the same core, the ISR
// merely preempts the loop, so volatile alone is sufficient and the module
// needs no locks. Other cores that want button events should receive them
// out of the event callback (app-level IPC, e.g. a queue or semaphore).

#include "pico-button-events.h"

#include "hardware/gpio.h"

// timing windows (milliseconds), same options as the JS module.
// Defaults are tuned for fast, natural tapping: 30 ms debounce, with
// 150 ms windows for both the long-press ("pressed") threshold and the
// multi-click chaining window. Loosen them (e.g. 30 / 500 / 500) for a
// slower, more deliberate feel.
static button_timing_t g_timing = {
    .debounce_ms = 30,
    .pressed_ms = 150,
    .clicked_ms = 150,
    .chord_ms = 100,
};

void buttonGetTiming(button_timing_t *timing)
{
    *timing = g_timing;
}

void buttonSetTiming(const button_timing_t *timing)
{
    g_timing = *timing;
}

// states, mirroring the STATE_* constants in the JS module
enum
{
    STATE_IDLE = 1,
    STATE_PRESSED = 2,
    STATE_CLICKED = 3,
    STATE_CLICKED_PRESSED = 4,
    STATE_DOUBLE_CLICKED = 5,
    STATE_DOUBLE_CLICKED_PRESSED = 6,
    STATE_TRIPLE_CLICKED = 7,
    STATE_TRIPLE_CLICKED_PRESSED = 8,
    STATE_QUADRUPLE_CLICKED = 9,
    STATE_RELEASE_WAIT = 10,
};

typedef struct
{
    uint gpio;
    button_event_cb callback;
    bool inited;
    int state; // current state machine state (STATE_*)
    volatile bool edgePending; // set by the ISR: at least one raw edge since last tick
    bool debouncing;
    bool hasPendingEvent; // state machine timer armed, event not yet emitted
    absolute_time_t debounceUntil;
    absolute_time_t emitAt;
    bool lastPressed; // last settled level processed by the state machine
} button_t;

static button_t buttons[BUTTON_MAX];

// Chord groups: a set of consecutive buttons whose simultaneously-held members
// form one chord (a bitmask). See buttonChordInit() in the header.
typedef struct
{
    bool inited;
    uint first_index; // first button index in the group
    uint count;       // number of buttons in the group
    button_chord_mode_t mode;
    uint held_mask;      // buttons in the group currently down (settled)
    uint chord_mask;     // [release] union of buttons down during this down period
    bool dirty;          // [press] held set changed since the last settle emit
    absolute_time_t settleUntil; // [press] settle deadline
    button_chord_cb callback;
} chord_group_t;

static chord_group_t chord_groups[BUTTON_CHORD_MAX];

// GPIO interrupt handler (runs on the core that called buttonInit).
// Must stay tiny: only record that an edge occurred.
static void buttonISR(uint gpio, uint32_t event_mask)
{
    (void)event_mask;
    for (uint i = 0; i < BUTTON_MAX; i++)
        if (buttons[i].inited && buttons[i].gpio == gpio)
        {
            buttons[i].edgePending = true;
            return;
        }
}

void buttonInit(uint button_index, uint gpio, button_event_cb callback)
{
    button_t *b = &buttons[button_index];
    b->gpio = gpio;
    b->callback = callback;
    b->inited = true;
    b->state = STATE_IDLE;
    b->edgePending = false;
    b->debouncing = false;
    b->hasPendingEvent = false;
    b->lastPressed = false; // pull-up: idle = not pressed

    // input with internal pull-up (button pulls the GPIO to ground)
    gpio_init(gpio);
    gpio_set_dir(gpio, GPIO_IN);
    gpio_set_pulls(gpio, true, false);
    // both edges: the state machine needs press AND release transitions
    gpio_set_irq_enabled_with_callback(gpio,
                                       GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE,
                                       true, buttonISR);
}

void buttonChordInit(uint first_index, uint count, button_chord_cb callback,
                     button_chord_mode_t mode)
{
    for (uint g = 0; g < BUTTON_CHORD_MAX; g++)
    {
        chord_group_t *cg = &chord_groups[g];
        if (cg->inited)
            continue;
        cg->inited = true;
        cg->first_index = first_index;
        cg->count = count;
        cg->mode = mode;
        cg->held_mask = 0;
        cg->chord_mask = 0;
        cg->dirty = false;
        cg->callback = callback;
        return;
    }
    // No free group slot: the caller should ensure BUTTON_CHORD_MAX is large
    // enough. (Consistent with buttonInit(), which also trusts its index.)
}

// Emit the event for the current state and make the state transition that
// goes with it (mirrors emitState() in the JS module).
static void emitState(uint button_index, button_t *b)
{
    button_event_t event;

    switch (b->state)
    {
    case STATE_PRESSED:
        event = BUTTON_EVENT_PRESSED;
        b->state = STATE_RELEASE_WAIT;
        break;
    case STATE_CLICKED:
        event = BUTTON_EVENT_CLICKED;
        b->state = STATE_IDLE;
        break;
    case STATE_CLICKED_PRESSED:
        event = BUTTON_EVENT_CLICKED_PRESSED;
        b->state = STATE_RELEASE_WAIT;
        break;
    case STATE_DOUBLE_CLICKED:
        event = BUTTON_EVENT_DOUBLE_CLICKED;
        b->state = STATE_IDLE;
        break;
    case STATE_DOUBLE_CLICKED_PRESSED:
        event = BUTTON_EVENT_DOUBLE_CLICKED_PRESSED;
        b->state = STATE_RELEASE_WAIT;
        break;
    case STATE_TRIPLE_CLICKED:
        event = BUTTON_EVENT_TRIPLE_CLICKED;
        b->state = STATE_IDLE;
        break;
    case STATE_TRIPLE_CLICKED_PRESSED:
        event = BUTTON_EVENT_TRIPLE_CLICKED_PRESSED;
        b->state = STATE_RELEASE_WAIT;
        break;
    case STATE_QUADRUPLE_CLICKED:
        event = BUTTON_EVENT_QUADRUPLE_CLICKED;
        b->state = STATE_IDLE;
        break;
    case STATE_RELEASE_WAIT:
        event = BUTTON_EVENT_RELEASED;
        b->state = STATE_IDLE;
        break;
    default:
        return; // no event for this state
    }
    b->hasPendingEvent = false;
    if (b->callback)
        b->callback(button_index, event);
}

// Update a chord group on a settled (debounced) press/release. Both modes
// track the held set; release mode accumulates a chord and emits it when the
// last button is released, while press mode re-arms the settle window
// (buttonTickAll() emits the held set once it is stable). Called from
// processLevelChange(), so it is debounce-filtered and runs only on the
// ticking core.
static void updateChord(uint button_index, bool pressed)
{
    for (uint g = 0; g < BUTTON_CHORD_MAX; g++)
    {
        chord_group_t *cg = &chord_groups[g];
        if (!cg->inited)
            continue;
        if (button_index < cg->first_index ||
            button_index - cg->first_index >= cg->count)
            continue; // this button is not in the group
        uint bit = 1u << (button_index - cg->first_index);

        // apply the settled level change to the held set (skip no-ops)
        if (pressed)
        {
            if (cg->held_mask & bit)
                continue;
            cg->held_mask |= bit;
        }
        else
        {
            if (!(cg->held_mask & bit))
                continue;
            cg->held_mask &= ~bit;
        }

        if (cg->mode == BUTTON_CHORD_MODE_RELEASE)
        {
            // accumulate the chord; emit it when the last button is released
            if (pressed)
                cg->chord_mask |= bit;
            if (cg->held_mask == 0 && cg->chord_mask && cg->callback)
            {
                cg->callback(cg->chord_mask);
                cg->chord_mask = 0;
            }
        }
        else // BUTTON_CHORD_MODE_PRESS
        {
            // (re)arm the settle window; buttonTickAll() emits when it expires
            cg->dirty = true;
            cg->settleUntil =
                delayed_by_us(get_absolute_time(), (uint64_t)g_timing.chord_ms * 1000);
        }
    }
}

// State machine transition on a settled (debounced) press or release
// (mirrors processLastValue() in the JS module).
static void processLevelChange(button_t *b, bool pressed)
{
    // keep chord groups in step with the debounced press/release
    updateChord((uint)(b - buttons), pressed);

    if (pressed)
    {
        switch (b->state)
        {
        case STATE_CLICKED:
            b->state = STATE_CLICKED_PRESSED;
            break;
        case STATE_DOUBLE_CLICKED:
            b->state = STATE_DOUBLE_CLICKED_PRESSED;
            break;
        case STATE_TRIPLE_CLICKED:
            b->state = STATE_TRIPLE_CLICKED_PRESSED;
            break;
        default:
            b->state = STATE_PRESSED;
            break;
        }
        // if still held after the window, it is a "pressed" (long press)
        b->hasPendingEvent = true;
        b->emitAt = delayed_by_us(get_absolute_time(), (uint64_t)g_timing.pressed_ms * 1000);
    }
    else
    {
        switch (b->state)
        {
        case STATE_PRESSED:
            b->state = STATE_CLICKED;
            b->hasPendingEvent = true;
            b->emitAt = delayed_by_us(get_absolute_time(), (uint64_t)g_timing.clicked_ms * 1000);
            break;
        case STATE_CLICKED_PRESSED:
            b->state = STATE_DOUBLE_CLICKED;
            b->hasPendingEvent = true;
            b->emitAt = delayed_by_us(get_absolute_time(), (uint64_t)g_timing.clicked_ms * 1000);
            break;
        case STATE_DOUBLE_CLICKED_PRESSED:
            b->state = STATE_TRIPLE_CLICKED;
            b->hasPendingEvent = true;
            b->emitAt = delayed_by_us(get_absolute_time(), (uint64_t)g_timing.clicked_ms * 1000);
            break;
        case STATE_TRIPLE_CLICKED_PRESSED:
            // fourth release: emit immediately (no quadruple-click window)
            b->state = STATE_QUADRUPLE_CLICKED;
            emitState((uint)(b - buttons), b);
            break;
        case STATE_RELEASE_WAIT:
            // release after a long press
            emitState((uint)(b - buttons), b);
            break;
        default:
            break; // IDLE: ignore
        }
    }
}

void buttonTickAll(absolute_time_t now)
{
    for (uint i = 0; i < BUTTON_MAX; i++)
    {
        button_t *b = &buttons[i];
        if (!b->inited)
            continue;

        if (b->debouncing)
        {
            // debounce window still open: ignore further edges (bounce).
            // absolute_time_diff_us(a, b) == b - a, so (debounceUntil, now)
            // is >= 0 once now has reached the deadline.
            if (absolute_time_diff_us(b->debounceUntil, now) >= 0)
            {
                b->debouncing = false;
                b->edgePending = false;
                // re-read the settled level and advance the state machine
                bool pressed = !gpio_get(b->gpio); // pull-up: low = pressed
                if (pressed != b->lastPressed)
                {
                    b->lastPressed = pressed;
                    processLevelChange(b, pressed);
                }
            }
            continue;
        }

        if (b->edgePending)
        {
            // start (or restart) the debounce window; the settled level is
            // read when it expires
            b->edgePending = false;
            b->debouncing = true;
            b->debounceUntil = delayed_by_us(now, (uint64_t)g_timing.debounce_ms * 1000);
            continue;
        }

        // Safety net: if the pin's actual level disagrees with the last
        // level the state machine processed and no edge is pending (i.e. an
        // edge was somehow missed), resync here so the machine can never
        // get stuck out of step with the physical button.
        bool actual = !gpio_get(b->gpio);
        if (actual != b->lastPressed)
        {
            b->lastPressed = actual;
            processLevelChange(b, actual);
        }

        if (b->hasPendingEvent && absolute_time_diff_us(b->emitAt, now) >= 0)
            emitState(i, b);
    }

    // settle press-mode chords: emit the current held set once it has been
    // stable for the settle window and is non-empty
    for (uint g = 0; g < BUTTON_CHORD_MAX; g++)
    {
        chord_group_t *cg = &chord_groups[g];
        if (!cg->inited || cg->mode != BUTTON_CHORD_MODE_PRESS || !cg->dirty)
            continue;
        if (absolute_time_diff_us(cg->settleUntil, now) >= 0)
        {
            cg->dirty = false;
            if (cg->held_mask && cg->callback)
                cg->callback(cg->held_mask);
        }
    }
}

const char *buttonEventName(button_event_t event)
{
    switch (event)
    {
    case BUTTON_EVENT_PRESSED:
        return "pressed";
    case BUTTON_EVENT_CLICKED:
        return "clicked";
    case BUTTON_EVENT_CLICKED_PRESSED:
        return "clicked_pressed";
    case BUTTON_EVENT_DOUBLE_CLICKED:
        return "double_clicked";
    case BUTTON_EVENT_DOUBLE_CLICKED_PRESSED:
        return "double_clicked_pressed";
    case BUTTON_EVENT_TRIPLE_CLICKED:
        return "triple_clicked";
    case BUTTON_EVENT_TRIPLE_CLICKED_PRESSED:
        return "triple_clicked_pressed";
    case BUTTON_EVENT_QUADRUPLE_CLICKED:
        return "quadruple_clicked";
    case BUTTON_EVENT_RELEASED:
        return "released";
    default:
        return "?";
    }
}
