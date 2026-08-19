#include <lvgl.h>

#include <zephyr/kernel.h>
#include <stdio.h>
#include <stdint.h>
#include <ctype.h>

#include <zmk/display.h>
#include <zmk/event_manager.h>
#include <zmk/events/position_state_changed.h>
#include <zmk/events/wpm_state_changed.h>
#include <zmk/events/layer_state_changed.h>
#include <zmk/wpm.h>
#include <zmk/keymap.h>

#include <fonts.h>

extern const lv_image_dsc_t bongo_idle;
extern const lv_image_dsc_t bongo_left;
extern const lv_image_dsc_t bongo_right;

#define BONGO_TAP_HOLD_MS 120

static lv_obj_t *cat_img;
static lv_obj_t *wpm_label;
static lv_obj_t *layer_label;

static bool left_paw_next = true;

static void idle_work_handler(struct k_work *work) { lv_image_set_src(cat_img, &bongo_idle); }
static K_WORK_DELAYABLE_DEFINE(idle_work, idle_work_handler);

/* Paw tap on any key press (both halves: events run on the central) */

struct tap_state {
    bool pressed;
};

static void tap_update_cb(struct tap_state state) {
    if (!state.pressed) {
        return;
    }
    lv_image_set_src(cat_img, left_paw_next ? &bongo_left : &bongo_right);
    left_paw_next = !left_paw_next;
    k_work_schedule_for_queue(zmk_display_work_q(), &idle_work, K_MSEC(BONGO_TAP_HOLD_MS));
}

static struct tap_state tap_get_state(const zmk_event_t *eh) {
    if (eh == NULL) {
        return (struct tap_state){.pressed = false};
    }
    const struct zmk_position_state_changed *ev = as_zmk_position_state_changed(eh);
    return (struct tap_state){.pressed = (ev != NULL) ? ev->state : false};
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_bongo_tap, struct tap_state, tap_update_cb, tap_get_state)
ZMK_SUBSCRIPTION(widget_bongo_tap, zmk_position_state_changed);

/* WPM number */

struct wpm_state {
    uint8_t wpm;
};

static void wpm_update_cb(struct wpm_state state) {
    char text[4];
    snprintf(text, sizeof(text), "%d", state.wpm);
    lv_label_set_text(wpm_label, text);
}

static struct wpm_state wpm_get_state(const zmk_event_t *eh) {
    return (struct wpm_state){.wpm = zmk_wpm_get_state()};
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_bongo_wpm, struct wpm_state, wpm_update_cb, wpm_get_state)
ZMK_SUBSCRIPTION(widget_bongo_wpm, zmk_wpm_state_changed);

/* Layer name */

struct layer_state {
    uint8_t index;
};

static void layer_update_cb(struct layer_state state) {
    const char *layer_name = zmk_keymap_layer_name(zmk_keymap_layer_index_to_id(state.index));
    char text[32];

    if (layer_name && *layer_name) {
        snprintf(text, sizeof(text), "%s", layer_name);
    } else {
        snprintf(text, sizeof(text), "Layer %d", state.index);
    }

#if IS_ENABLED(CONFIG_PROSPECTOR_LAYER_NAME_UPPERCASE)
    for (int i = 0; text[i]; i++) {
        text[i] = toupper((unsigned char)text[i]);
    }
#endif

    lv_label_set_text(layer_label, text);
}

static struct layer_state layer_get_state(const zmk_event_t *eh) {
    return (struct layer_state){.index = zmk_keymap_highest_layer_active()};
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_bongo_layer, struct layer_state, layer_update_cb,
                            layer_get_state)
ZMK_SUBSCRIPTION(widget_bongo_layer, zmk_layer_state_changed);

lv_obj_t *zmk_display_status_screen() {
    lv_obj_t *screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen, 255, LV_PART_MAIN);

    layer_label = lv_label_create(screen);
    lv_label_set_text(layer_label, "");
    lv_obj_set_style_text_font(layer_label, &DINish_Medium_24, LV_PART_MAIN);
    lv_obj_set_style_text_color(layer_label, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_align(layer_label, LV_ALIGN_TOP_MID, 0, 24);

    cat_img = lv_image_create(screen);
    lv_image_set_src(cat_img, &bongo_idle);
    lv_obj_align(cat_img, LV_ALIGN_CENTER, 0, 8);

    wpm_label = lv_label_create(screen);
    lv_label_set_text(wpm_label, "0");
    lv_obj_set_style_text_font(wpm_label, &FR_Medium_32, LV_PART_MAIN);
    lv_obj_set_style_text_color(wpm_label, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_align(wpm_label, LV_ALIGN_BOTTOM_MID, 0, -20);

    widget_bongo_tap_init();
    widget_bongo_wpm_init();
    widget_bongo_layer_init();

    return screen;
}
