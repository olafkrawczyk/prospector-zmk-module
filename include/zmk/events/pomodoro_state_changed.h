#pragma once

#include <zephyr/kernel.h>
#include <zmk/event_manager.h>
#include <zmk/pomodoro.h>

struct zmk_pomodoro_state_changed {
    enum zmk_pomodoro_state state;
    enum zmk_pomodoro_transition transition;
    uint8_t preset;
    uint16_t remaining_sec;
};

ZMK_EVENT_DECLARE(zmk_pomodoro_state_changed);
