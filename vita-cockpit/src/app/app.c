#include "app/app.h"

#include <psp2/ctrl.h>
#include <psp2/kernel/processmgr.h>
#include <string.h>
#include <vita2d.h>

#include "app/screen.h"
#include "core/metrics.h"
#include "core/plugins.h"
#include "gfx/layout.h"
#include "panels/registry.h"

/*
 * Lifecycle, time, scheduling, and scroll input. This file owns the frame loop
 * and nothing else: what is drawn and in what order lives in screen.c, and how
 * anything looks lives in gfx/theme.
 *
 * The controller is used for one thing: scrolling the panel area. There is still
 * no in-app exit button (D39) -- a dashboard is meant to be left running, and the
 * PS button is the way out.
 */

#define MICROS_PER_SECOND 1000000.0

/* Analog dead zone; below/above these the stick scrolls proportionally. */
#define STICK_LOW 96
#define STICK_HIGH 160

static float scroll_input(const SceCtrlData *pad)
{
    float dir = 0.0f;

    if ((pad->buttons & SCE_CTRL_UP) != 0)
        dir -= 1.0f;
    if ((pad->buttons & SCE_CTRL_DOWN) != 0)
        dir += 1.0f;

    if (pad->ly < STICK_LOW)
        dir -= (float)(STICK_LOW - (int)pad->ly) / (float)STICK_LOW;
    else if (pad->ly > STICK_HIGH)
        dir += (float)((int)pad->ly - STICK_HIGH) / (float)(255 - STICK_HIGH);

    if (dir < -1.0f)
        dir = -1.0f;
    if (dir > 1.0f)
        dir = 1.0f;

    return dir;
}

static float clamp_scroll(float scroll)
{
    float max_scroll = (float)layout_max_scroll();

    if (scroll < 0.0f)
        return 0.0f;
    if (scroll > max_scroll)
        return max_scroll;

    return scroll;
}

int app_run(void)
{
    /* Analog is needed for the scroll stick; digital alone would quantise it. */
    (void)sceCtrlSetSamplingMode(SCE_CTRL_MODE_ANALOG);

    metrics_init();
    plugins_scan();
    layout_update();

    uint64_t last = sceKernelGetProcessTimeWide();
    double slow_acc = 0.0;
    float scroll = 0.0f;

    for (;;) {
        uint64_t now = sceKernelGetProcessTimeWide();
        float dt = (float)((double)(now - last) / MICROS_PER_SECOND);
        last = now;

        if (dt > MAX_FRAME_DT)
            dt = MAX_FRAME_DT;

        SceCtrlData pad;
        memset(&pad, 0, sizeof(pad));

        /* A failed read leaves the pad zeroed: the screen keeps its position
         * rather than jumping, which is the safe direction (rule 6). */
        (void)sceCtrlPeekBufferPositive(0, &pad, 1);

        scroll = clamp_scroll(scroll + scroll_input(&pad) * SCROLL_SPEED * dt);

        metrics_sample_frame(dt);

        /*
         * Bounded catch-up. An unbounded loop would repay a long stall with one
         * blocking sample per 100 ms inside a single frame, and each iteration
         * pushes into the 200-sample ring -- so a stall would overwrite the
         * whole 20-second trace with duplicates of the current value (D25).
         */
        slow_acc += (double)dt;

        int caught_up = 0;
        while (slow_acc >= SLOW_SAMPLE_PERIOD &&
               caught_up < MAX_CATCH_UP_SAMPLES) {
            metrics_sample_slow();
            slow_acc -= SLOW_SAMPLE_PERIOD;
            ++caught_up;
        }

        if (slow_acc >= SLOW_SAMPLE_PERIOD)
            slow_acc = 0.0;

        vita2d_start_drawing();
        vita2d_clear_screen();
        screen_draw((float)((double)now / MICROS_PER_SECOND), scroll);
        vita2d_end_drawing();
        vita2d_swap_buffers();
    }
}
