#pragma once

#include <stdint.h>

/*
 * Step the display backlight brightness by `delta` percentage points,
 * clamped to [1, 100]. Returns the new brightness value.
 */
uint8_t prospector_brightness_step(int8_t delta);

/* Current backlight brightness (1..100). */
uint8_t prospector_brightness_get(void);

/*
 * Blink the backlight `times` times (dim/restore), then return to the
 * brightness that was active when the blink started. Used by the pomodoro
 * timer to flag the end of a phase. Re-entrant-safe: a blink in progress
 * is ignored.
 */
void prospector_brightness_blink(uint8_t times);
