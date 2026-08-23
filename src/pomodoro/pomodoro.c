#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <zmk/pomodoro.h>
#include <zmk/events/pomodoro_state_changed.h>
#include <zmk/event_manager.h>

LOG_MODULE_REGISTER(zmk_pomodoro, CONFIG_ZMK_LOG_LEVEL);

static const uint16_t preset_work_sec[ZMK_POMODORO_PRESET_COUNT] = {25 * 60, 50 * 60, 60};
static const uint16_t preset_break_sec[ZMK_POMODORO_PRESET_COUNT] = {5 * 60, 10 * 60, 10};

static struct k_work_delayable pomodoro_tick_work;

static enum zmk_pomodoro_state pom_state = ZMK_POMODORO_STATE_IDLE;
static uint8_t pom_preset = 0;
static uint16_t pom_remaining_sec = 25 * 60; /* initialised to preset 0's work time */

static void emit(enum zmk_pomodoro_transition transition) {
    LOG_DBG("pomodoro: state=%u preset=%u remaining=%u transition=%u", pom_state, pom_preset,
            pom_remaining_sec, transition);

    raise_zmk_pomodoro_state_changed(
        (struct zmk_pomodoro_state_changed){
            .state = pom_state, .transition = transition, .preset = pom_preset,
            .remaining_sec = pom_remaining_sec});
}

static void reschedule_tick(void) {
    /* 1 Hz tick while running. */
    k_work_reschedule(&pomodoro_tick_work, K_SECONDS(1));
}

static void tick_handler(struct k_work *work) {
    ARG_UNUSED(work);

    if (pom_state == ZMK_POMODORO_STATE_IDLE) {
        return;
    }

    if (pom_remaining_sec > 0) {
        pom_remaining_sec--;
    }

    if (pom_remaining_sec > 0) {
        emit(ZMK_POMODORO_TRANSITION_TICK);
        reschedule_tick();
        return;
    }

    /* Phase elapsed. */
    if (pom_state == ZMK_POMODORO_STATE_WORK) {
        /* Work ended -> auto-start the break. */
        pom_state = ZMK_POMODORO_STATE_PAUSE;
        pom_remaining_sec = preset_break_sec[pom_preset];
        emit(ZMK_POMODORO_TRANSITION_WORK_ENDED);
        reschedule_tick();
    } else { /* PAUSE */
        /* Break ended -> idle (no infinite looping). */
        pom_state = ZMK_POMODORO_STATE_IDLE;
        pom_remaining_sec = preset_work_sec[pom_preset];
        emit(ZMK_POMODORO_TRANSITION_PAUSE_ENDED);
        /* No further ticks while idle. */
    }
}

void zmk_pomodoro_start_stop(void) {
    if (pom_state == ZMK_POMODORO_STATE_IDLE) {
        /* Start: begin the work phase of the selected preset. */
        pom_state = ZMK_POMODORO_STATE_WORK;
        pom_remaining_sec = preset_work_sec[pom_preset];
        emit(ZMK_POMODORO_TRANSITION_STARTED);
        reschedule_tick();
    } else {
        /* Stop from either running phase -> idle. */
        (void)k_work_cancel_delayable(&pomodoro_tick_work);
        pom_state = ZMK_POMODORO_STATE_IDLE;
        pom_remaining_sec = preset_work_sec[pom_preset];
        emit(ZMK_POMODORO_TRANSITION_STOPPED);
    }
}

void zmk_pomodoro_cycle_preset(void) {
    /* Only meaningful while idle; ignore while a phase is running. */
    if (pom_state != ZMK_POMODORO_STATE_IDLE) {
        return;
    }

    pom_preset = (pom_preset + 1) % ZMK_POMODORO_PRESET_COUNT;
    pom_remaining_sec = preset_work_sec[pom_preset];
    emit(ZMK_POMODORO_TRANSITION_PRESET_CHANGED);
}

struct zmk_pomodoro_snapshot zmk_pomodoro_get_snapshot(void) {
    return (struct zmk_pomodoro_snapshot){
        .state = pom_state,
        .preset = pom_preset,
        .remaining_sec = pom_remaining_sec,
        .work_sec = preset_work_sec[pom_preset],
        .break_sec = preset_break_sec[pom_preset],
    };
}

static int pomodoro_init(const struct device *dev) {
    ARG_UNUSED(dev);
    k_work_init_delayable(&pomodoro_tick_work, tick_handler);
    return 0;
}

SYS_INIT(pomodoro_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
