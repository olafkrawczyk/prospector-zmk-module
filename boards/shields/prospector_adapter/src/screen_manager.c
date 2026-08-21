/*
 * Screen manager: creates the enabled status screen layouts and, with
 * CONFIG_PROSPECTOR_SWIPE_NAVIGATION, switches between them on horizontal
 * swipe gestures from the touchscreen (LVGL pointer gestures).
 */

#include <lvgl.h>
#include <stdint.h>
#include <zephyr/kernel.h>

#include <zmk/display/status_screen.h>

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

/* Fixed IDs matching the layout order in screens[] below. */
#define SCREEN_ID_CLASSIC 0
#define SCREEN_ID_RADII 1
#define SCREEN_ID_FIELD 2
#define SCREEN_ID_OPERATOR 3
#define SCREEN_ID_BONGO 4

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
     IS_ENABLED(CONFIG_PROSPECTOR_SCREEN_BONGO_ENABLED))

#if IS_ENABLED(CONFIG_PROSPECTOR_SWIPE_NAVIGATION) && (N_ENABLED > 1)
#define SWIPE_NAV_ACTIVE 1
#endif

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

#ifdef SWIPE_NAV_ACTIVE

static uint8_t current_screen;

static void gesture_event_cb(lv_event_t *e) {
    (void)e;
    lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_active());
    uint8_t next = current_screen;
    lv_screen_load_anim_t anim;

    if (dir == LV_DIR_LEFT) {
        next = (current_screen + 1) % N_ENABLED;
        anim = LV_SCR_LOAD_ANIM_MOVE_LEFT;
    } else if (dir == LV_DIR_RIGHT) {
        next = (current_screen + N_ENABLED - 1) % N_ENABLED;
        anim = LV_SCR_LOAD_ANIM_MOVE_RIGHT;
    } else {
        return;
    }

    current_screen = next;
    lv_screen_load_anim(screens[next].obj, anim, 250, 0, false);
}

/* Gesture events are delivered to the pressed object and bubble up only
 * through objects carrying LV_OBJ_FLAG_GESTURE_BUBBLE. Flag every descendant
 * (but not the screen root itself, or the event would bubble past it into
 * nowhere) so a swipe lands on the screen's handler regardless of which
 * widget was touched. */
static void add_gesture_bubble_recursive(lv_obj_t *obj) {
    lv_obj_add_flag(obj, LV_OBJ_FLAG_GESTURE_BUBBLE);
    uint32_t child_count = lv_obj_get_child_count(obj);
    for (uint32_t i = 0; i < child_count; i++) {
        add_gesture_bubble_recursive(lv_obj_get_child(obj, i));
    }
}

static void attach_gesture_handler(lv_obj_t *screen) {
    lv_obj_add_event_cb(screen, gesture_event_cb, LV_EVENT_GESTURE, NULL);
    uint32_t child_count = lv_obj_get_child_count(screen);
    for (uint32_t i = 0; i < child_count; i++) {
        add_gesture_bubble_recursive(lv_obj_get_child(screen, i));
    }
}

#endif /* SWIPE_NAV_ACTIVE */

lv_obj_t *zmk_display_status_screen(void) {
    uint8_t default_idx = default_screen_index();

    /* Guard against a boot screen that isn't compiled in. */
    if (default_idx >= N_ENABLED) {
        default_idx = 0;
    }

#ifdef SWIPE_NAV_ACTIVE
    for (uint8_t i = 0; i < N_ENABLED; i++) {
        screens[i].obj = screens[i].create();
        attach_gesture_handler(screens[i].obj);
    }
    current_screen = default_idx;
#else
    /* Single screen or no touch: keep heap usage as before. */
    screens[default_idx].obj = screens[default_idx].create();
#endif

    return screens[default_idx].obj;
}
