#ifndef COCKPIT_APP_APP_H
#define COCKPIT_APP_APP_H

/*
 * Lifecycle and scheduling. Screen geometry lives in gfx/layout.h (D24) and
 * region rects in panels/registry.c, because an asymmetric layout cannot be
 * derived from a grid index.
 */

/* Slow sampling cadence in seconds (D10). 10 Hz. */
#define SLOW_SAMPLE_PERIOD 0.1

/* Scroll rate at full stick or d-pad deflection, pixels per second. */
#define SCROLL_SPEED 900.0f

/*
 * A gap longer than this is a stall -- suspend/resume, a blocking syscall --
 * not a slow frame. Clamping it keeps one stall from being repaid as a burst
 * of slow samples inside a single frame (D25).
 */
#define MAX_FRAME_DT 0.5f

/* Ceiling on catch-up samples per frame, so the debt is dropped, not replayed. */
#define MAX_CATCH_UP_SAMPLES 3

/* Returns 0 on a clean exit. */
int app_run(void);

#endif
