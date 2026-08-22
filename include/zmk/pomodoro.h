#pragma once

#include <stdint.h>
#include <stdbool.h>

/*
 * Pomodoro timer service — a tiny decoupled state machine driven by a
 * 1 Hz k_work_delayable. The screen only renders the current state; the
 * countdown keeps running while other screens are shown.
 *
 * Two fixed interval presets (work / break, in minutes):
 *   preset 0 -> 25 / 5
 *   preset 1 -> 50 / 10
 *
 * State machine:
 *   IDLE  -- start -->        WORK   (uses the selected preset's work time)
 *   IDLE  -- switch preset --> cycle preset 0 <-> 1
 *   WORK  -- stop -->         IDLE
 *   WORK  -- work elapsed --> PAUSE  (auto-started break)
 *   PAUSE -- stop -->         IDLE
 *   PAUSE -- break elapsed -->IDLE
 */

enum zmk_pomodoro_state {
    ZMK_POMODORO_STATE_IDLE = 0,
    ZMK_POMODORO_STATE_WORK = 1,
    ZMK_POMODORO_STATE_PAUSE = 2,
};

enum zmk_pomodoro_transition {
    ZMK_POMODORO_TRANSITION_TICK = 0,
    ZMK_POMODORO_TRANSITION_STARTED = 1,
    ZMK_POMODORO_TRANSITION_STOPPED = 2,
    ZMK_POMODORO_TRANSITION_WORK_ENDED = 3,
    ZMK_POMODORO_TRANSITION_PAUSE_ENDED = 4,
    ZMK_POMODORO_TRANSITION_PRESET_CHANGED = 5,
};

#define ZMK_POMODORO_PRESET_COUNT 2

struct zmk_pomodoro_snapshot {
    enum zmk_pomodoro_state state;
    uint8_t preset;          /* 0 = 25/5, 1 = 50/10 */
    uint16_t remaining_sec;  /* seconds left in the current phase */
    uint16_t work_sec;       /* selected preset's work duration */
    uint16_t break_sec;      /* selected preset's break duration */
};

/* Keymap behaviors call these. */
void zmk_pomodoro_start_stop(void);
void zmk_pomodoro_cycle_preset(void);

/* Read-only snapshot for the screen / listeners. */
struct zmk_pomodoro_snapshot zmk_pomodoro_get_snapshot(void);
