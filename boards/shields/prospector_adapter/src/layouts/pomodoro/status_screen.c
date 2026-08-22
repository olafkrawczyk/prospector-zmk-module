/*
 * Pomodoro screen — clean, Apple-style: black background, white thin digits,
 * a small uppercase caption above. Everything centred vertically and
 * horizontally. Renders the snapshot published by the pomodoro service
 * (src/pomodoro/pomodoro.c); the timer keeps running while other screens
 * are shown.
 */

#include <lvgl.h>
#include <stdio.h>

#include <zmk/pomodoro.h>
#include <zmk/events/pomodoro_state_changed.h>
#include <zmk/event_manager.h>
#include <zmk/display.h>

#include <fonts.h>

static lv_obj_t *time_label;
static lv_obj_t *state_label;
static lv_obj_t *flash_overlay;

/* Fixed label width so digit-width differences don't make the timer jump. */
#define TIME_LABEL_W 200

static const char *state_caption(enum zmk_pomodoro_state s) {
    switch (s) {
    case ZMK_POMODORO_STATE_WORK:
        return "FOCUS";
    case ZMK_POMODORO_STATE_PAUSE:
        return "BREAK";
    default:
        return "READY";
    }
}

static void format_remaining(uint16_t remaining_sec, char *out, size_t len) {
    uint16_t m = remaining_sec / 60;
    uint16_t s = remaining_sec % 60;
    snprintf(out, len, "%u:%02u", m, s);
}

static void set_flash_opa(void *obj, int32_t v) {
    lv_obj_set_style_opa((lv_obj_t *)obj, (lv_opa_t)v, LV_PART_MAIN);
}

static void trigger_red_flash(void) {
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, flash_overlay);
    lv_anim_set_values(&a, 0, 220);
    lv_anim_set_time(&a, 250);
    lv_anim_set_playback_time(&a, 250);
    lv_anim_set_repeat_count(&a, 3);
    lv_anim_set_exec_cb(&a, set_flash_opa);
    lv_anim_start(&a);
}

static void pomodoro_render(struct zmk_pomodoro_state_changed st) {
    char time_text[8];
    format_remaining(st.remaining_sec, time_text, sizeof(time_text));
    lv_label_set_text(time_label, time_text);
    lv_label_set_text(state_label, state_caption(st.state));

    if (st.transition == ZMK_POMODORO_TRANSITION_WORK_ENDED ||
        st.transition == ZMK_POMODORO_TRANSITION_PAUSE_ENDED) {
        trigger_red_flash();
    }
}

static struct zmk_pomodoro_state_changed pomodoro_state_from_event(const zmk_event_t *eh) {
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

ZMK_DISPLAY_WIDGET_LISTENER(pomodoro_screen, struct zmk_pomodoro_state_changed,
                            pomodoro_render, pomodoro_state_from_event)
ZMK_SUBSCRIPTION(pomodoro_screen, zmk_pomodoro_state_changed);

lv_obj_t *zmk_prospector_screen_pomodoro_create() {
    lv_obj_t *screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen, 255, LV_PART_MAIN);
    lv_obj_set_style_border_width(screen, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(screen, 0, LV_PART_MAIN);

    /* Small uppercase caption, centred, sitting above the timer. */
    state_label = lv_label_create(screen);
    lv_label_set_text(state_label, "READY");
    lv_obj_set_style_text_font(state_label, &FG_Medium_20, LV_PART_MAIN);
    lv_obj_set_style_text_color(state_label, lv_color_hex(0x8E8E93), LV_PART_MAIN);
    lv_obj_set_style_text_letter_space(state_label, 6, LV_PART_MAIN);
    lv_obj_align(state_label, LV_ALIGN_CENTER, 0, -36);

    /* Big thin countdown, centred.  Fixed width + center text-align keeps
     * the digit midpoint stable when individual glyph widths differ. */
    time_label = lv_label_create(screen);
    lv_label_set_text(time_label, "25:00");
    lv_obj_set_style_text_font(time_label, &PPF_NarrowThin_64, LV_PART_MAIN);
    lv_obj_set_style_text_color(time_label, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_width(time_label, TIME_LABEL_W);
    lv_obj_set_style_text_align(time_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_align(time_label, LV_ALIGN_CENTER, 0, 14);

    /* Full-screen red flash overlay (hidden until a phase ends). */
    flash_overlay = lv_obj_create(screen);
    lv_obj_set_size(flash_overlay, 280, 240);
    lv_obj_set_style_bg_color(flash_overlay, lv_color_hex(0xE60000), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(flash_overlay, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(flash_overlay, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(flash_overlay, 0, LV_PART_MAIN);
    lv_obj_set_style_opa(flash_overlay, 0, LV_PART_MAIN);
    lv_obj_center(flash_overlay);

    /* Populate initial state from the service. */
    pomodoro_screen_init();

    return screen;
}
