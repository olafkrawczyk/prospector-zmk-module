/*
 * Screen manager: creates the enabled status screen layouts and, with
 * CONFIG_PROSPECTOR_TOUCHSCREEN, reacts to touchscreen gestures:
 *   - swipe left/right  : cycle between enabled screens (slide animation)
 *   - swipe up/down     : raise/lower display brightness
 *
 * Widgets are treated as inert (no click/scroll): all LV_OBJ_FLAG_CLICKABLE
 * and LV_OBJ_FLAG_SCROLLABLE flags are stripped from descendants so touches
 * only produce gestures, never widget interaction.
 */

#include <lvgl.h>
#include <stdint.h>
#include <zephyr/kernel.h>

#include <zmk/display/status_screen.h>
#include <prospector_brightness.h>

#if IS_ENABLED(CONFIG_PROSPECTOR_SCREEN_CLASSIC_ENABLED)
lv_obj_t *zmk_prospector_screen_classic_create(void);
#endif
#if IS_ENABLED(CONFIG_PROSPECTOR_SCREEN_RADII_ENABLED)
lv_obj_t *zmk_prospector_screen_radii_create(void);
#endif
#if IS_ENABLED(CONFIG_PROSPECTOR_SCREEN_FIELD_ENABLED)
lv_obj_t *zmk_prospector_screen_field_create(void);
#endif
#if IS_ENABLED(CONFIG_PROSPECTOR_SCREEN_OPERATOR_ENABLED)
lv_obj_t *zmk_prospector_screen_operator_create(void);
#endif
#if IS_ENABLED(CONFIG_PROSPECTOR_SCREEN_BONGO_ENABLED)
lv_obj_t *zmk_prospector_screen_bongo_create(void);
#endif
#if IS_ENABLED(CONFIG_PROSPECTOR_SCREEN_POMODORO_ENABLED)
lv_obj_t *zmk_prospector_screen_pomodoro_create(void);
#endif

/* Fixed IDs matching the layout order in screens[] below. */
#define SCREEN_ID_CLASSIC 0
#define SCREEN_ID_RADII 1
#define SCREEN_ID_FIELD 2
#define SCREEN_ID_OPERATOR 3
#define SCREEN_ID_BONGO 4
#define SCREEN_ID_POMODORO 5

/* Boot screen, from the PROSPECTOR_STATUS_SCREEN_* choice. */
#if defined(CONFIG_PROSPECTOR_STATUS_SCREEN_CLASSIC)
#define DEFAULT_SCREEN_ID SCREEN_ID_CLASSIC
#elif defined(CONFIG_PROSPECTOR_STATUS_SCREEN_RADII)
#define DEFAULT_SCREEN_ID SCREEN_ID_RADII
#elif defined(CONFIG_PROSPECTOR_STATUS_SCREEN_FIELD)
#define DEFAULT_SCREEN_ID SCREEN_ID_FIELD
#elif defined(CONFIG_PROSPECTOR_STATUS_SCREEN_OPERATOR)
#define DEFAULT_SCREEN_ID SCREEN_ID_OPERATOR
#elif defined(CONFIG_PROSPECTOR_STATUS_SCREEN_BONGO)
#define DEFAULT_SCREEN_ID SCREEN_ID_BONGO
#endif

#define N_ENABLED                                                                                  \
    (IS_ENABLED(CONFIG_PROSPECTOR_SCREEN_CLASSIC_ENABLED) +                                        \
     IS_ENABLED(CONFIG_PROSPECTOR_SCREEN_RADII_ENABLED) +                                          \
     IS_ENABLED(CONFIG_PROSPECTOR_SCREEN_FIELD_ENABLED) +                                          \
     IS_ENABLED(CONFIG_PROSPECTOR_SCREEN_OPERATOR_ENABLED) +                                       \
     IS_ENABLED(CONFIG_PROSPECTOR_SCREEN_BONGO_ENABLED) +                                          \
     IS_ENABLED(CONFIG_PROSPECTOR_SCREEN_POMODORO_ENABLED))

#if IS_ENABLED(CONFIG_PROSPECTOR_TOUCHSCREEN)
#define TOUCH_ACTIVE 1
#if (N_ENABLED > 1)
#define MULTI_SCREEN 1
#endif
#endif

#define BRIGHTNESS_STEP 10

struct screen_entry {
    lv_obj_t *(*create)(void);
    lv_obj_t *obj;
};

/* Swipe order: left/right cycles through this table (with wraparound). */
static struct screen_entry screens[] = {
#if IS_ENABLED(CONFIG_PROSPECTOR_SCREEN_CLASSIC_ENABLED)
    {.create = zmk_prospector_screen_classic_create},
#endif
#if IS_ENABLED(CONFIG_PROSPECTOR_SCREEN_RADII_ENABLED)
    {.create = zmk_prospector_screen_radii_create},
#endif
#if IS_ENABLED(CONFIG_PROSPECTOR_SCREEN_FIELD_ENABLED)
    {.create = zmk_prospector_screen_field_create},
#endif
#if IS_ENABLED(CONFIG_PROSPECTOR_SCREEN_OPERATOR_ENABLED)
    {.create = zmk_prospector_screen_operator_create},
#endif
#if IS_ENABLED(CONFIG_PROSPECTOR_SCREEN_BONGO_ENABLED)
    {.create = zmk_prospector_screen_bongo_create},
#endif
#if IS_ENABLED(CONFIG_PROSPECTOR_SCREEN_POMODORO_ENABLED)
    {.create = zmk_prospector_screen_pomodoro_create},
#endif
};

/* Map the default screen ID onto the compacted screens[] table:
 * index = ID minus the number of compiled-out screens with a lower ID. */
static uint8_t default_screen_index(void) {
    uint8_t idx = DEFAULT_SCREEN_ID;

#if !IS_ENABLED(CONFIG_PROSPECTOR_SCREEN_CLASSIC_ENABLED)
    if (DEFAULT_SCREEN_ID > SCREEN_ID_CLASSIC) { idx--; }
#endif
#if !IS_ENABLED(CONFIG_PROSPECTOR_SCREEN_RADII_ENABLED)
    if (DEFAULT_SCREEN_ID > SCREEN_ID_RADII) { idx--; }
#endif
#if !IS_ENABLED(CONFIG_PROSPECTOR_SCREEN_FIELD_ENABLED)
    if (DEFAULT_SCREEN_ID > SCREEN_ID_FIELD) { idx--; }
#endif
#if !IS_ENABLED(CONFIG_PROSPECTOR_SCREEN_OPERATOR_ENABLED)
    if (DEFAULT_SCREEN_ID > SCREEN_ID_OPERATOR) { idx--; }
#endif
    return idx;
}

#ifdef TOUCH_ACTIVE

static uint8_t current_screen;

static void gesture_event_cb(lv_event_t *e) {
    (void)e;
    lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_active());

    switch (dir) {
#ifdef MULTI_SCREEN
    case LV_DIR_LEFT:
        current_screen = (current_screen + 1) % N_ENABLED;
        lv_screen_load_anim(screens[current_screen].obj, LV_SCR_LOAD_ANIM_MOVE_LEFT, 250, 0, false);
        break;
    case LV_DIR_RIGHT:
        current_screen = (current_screen + N_ENABLED - 1) % N_ENABLED;
        lv_screen_load_anim(screens[current_screen].obj, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 250, 0, false);
        break;
#endif
    case LV_DIR_TOP:
        prospector_brightness_step(BRIGHTNESS_STEP);
        break;
    case LV_DIR_BOTTOM:
        prospector_brightness_step(-BRIGHTNESS_STEP);
        break;
    default:
        break;
    }
}

/* Strip click/scroll flags so widgets are inert to touch (treated as images).
 * The screen root keeps CLICKABLE so it is the pointer hit-target that feeds
 * gesture detection; GESTURE_BUBBLE lets events from any child reach it. */
static void make_inert_recursive(lv_obj_t *obj) {
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(obj, LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLL_CHAIN_VER | LV_OBJ_FLAG_SCROLL_CHAIN_HOR);
    uint32_t child_count = lv_obj_get_child_count(obj);
    for (uint32_t i = 0; i < child_count; i++) {
        make_inert_recursive(lv_obj_get_child(obj, i));
    }
}

static void attach_gesture_handler(lv_obj_t *screen) {
    lv_obj_add_event_cb(screen, gesture_event_cb, LV_EVENT_GESTURE, NULL);
    /* Root stays clickable (hit-target), but not scrollable. */
    lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    uint32_t child_count = lv_obj_get_child_count(screen);
    for (uint32_t i = 0; i < child_count; i++) {
        make_inert_recursive(lv_obj_get_child(screen, i));
    }
}

#endif /* TOUCH_ACTIVE */

#if IS_ENABLED(CONFIG_PROSPECTOR_POMODORO)

#include <zmk/display.h>
#include <zmk/pomodoro.h>
#include <zmk/events/pomodoro_state_changed.h>

/* Index of the pomodoro screen inside the compacted screens[] table:
 * count of compiled-in screens that come before it (all non-pomodoro). */
#define POMODORO_SCREEN_INDEX                                                                      \
    (IS_ENABLED(CONFIG_PROSPECTOR_SCREEN_CLASSIC_ENABLED) +                                        \
     IS_ENABLED(CONFIG_PROSPECTOR_SCREEN_RADII_ENABLED) +                                          \
     IS_ENABLED(CONFIG_PROSPECTOR_SCREEN_FIELD_ENABLED) +                                          \
     IS_ENABLED(CONFIG_PROSPECTOR_SCREEN_OPERATOR_ENABLED) +                                       \
     IS_ENABLED(CONFIG_PROSPECTOR_SCREEN_BONGO_ENABLED))

static void pomodoro_ui_update_cb(struct zmk_pomodoro_state_changed st) {
    bool is_phase_end = (st.transition == ZMK_POMODORO_TRANSITION_WORK_ENDED ||
                         st.transition == ZMK_POMODORO_TRANSITION_PAUSE_ENDED);
    bool is_started = (st.transition == ZMK_POMODORO_TRANSITION_STARTED);

    if (!is_phase_end && !is_started) {
        return;
    }

    /* Blink only on phase end, not on start. */
    if (is_phase_end) {
        prospector_brightness_blink(3);
    }

#if defined(TOUCH_ACTIVE) && IS_ENABLED(CONFIG_PROSPECTOR_SCREEN_POMODORO_ENABLED)
    if (current_screen != POMODORO_SCREEN_INDEX) {
        current_screen = POMODORO_SCREEN_INDEX;
        lv_screen_load_anim(screens[current_screen].obj, LV_SCR_LOAD_ANIM_FADE_IN, 250, 0, false);
    }
#endif
}

static struct zmk_pomodoro_state_changed pomodoro_ui_from_event(const zmk_event_t *eh) {
    const struct zmk_pomodoro_state_changed *ev = as_zmk_pomodoro_state_changed(eh);
    if (ev == NULL) {
        struct zmk_pomodoro_snapshot snap = zmk_pomodoro_get_snapshot();
        return (struct zmk_pomodoro_state_changed){
            .state = snap.state,
            .transition = ZMK_POMODORO_TRANSITION_TICK,
            .preset = snap.preset,
            .remaining_sec = snap.remaining_sec,
        };
    }
    return *ev;
}

ZMK_DISPLAY_WIDGET_LISTENER(pomodoro_ui, struct zmk_pomodoro_state_changed, pomodoro_ui_update_cb,
                            pomodoro_ui_from_event)
ZMK_SUBSCRIPTION(pomodoro_ui, zmk_pomodoro_state_changed);

#endif /* CONFIG_PROSPECTOR_POMODORO */

lv_obj_t *zmk_display_status_screen(void) {
    uint8_t default_idx = default_screen_index();

    /* Guard against a boot screen that isn't compiled in. */
    if (default_idx >= N_ENABLED) {
        default_idx = 0;
    }

#ifdef TOUCH_ACTIVE
    for (uint8_t i = 0; i < N_ENABLED; i++) {
        screens[i].obj = screens[i].create();
        attach_gesture_handler(screens[i].obj);
    }
    current_screen = default_idx;
#else
    /* Single screen or no touch: keep heap usage as before. */
    screens[default_idx].obj = screens[default_idx].create();
#endif

#if IS_ENABLED(CONFIG_PROSPECTOR_POMODORO)
    /* Prime the pomodoro UI listener (blink + auto-switch on phase end). */
    pomodoro_ui_init();
#endif

    return screens[default_idx].obj;
}
