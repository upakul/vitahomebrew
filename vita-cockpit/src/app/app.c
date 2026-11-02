#include "app/app.h"

#include <psp2/ctrl.h>
#include <psp2/kernel/processmgr.h>
#include <string.h>
#include <vita2d.h>

#include "app/screen.h"
#include "ui/ui_state.h"
#include "core/input.h"
#include "core/metrics.h"
#include "core/plugin_editor.h"
#include "core/plugins.h"
#include "gfx/layout.h"
#include "panels/plugins.h"
#include "panels/registry.h"
#include "ui/modal.h"
#include <stdio.h>

/*
 * Lifecycle, time, scheduling, and input. This file owns the frame loop and
 * the one mutable UiState. What is drawn lives in screen.c; how anything looks
 * lives in gfx/theme.
 */

#define MICROS_PER_SECOND 1000000.0

/* Analog dead zones for the list cursor. */
#define STICK_LOW 70
#define STICK_HIGH 185

#define HOME_SCROLL_STEP 60.0f

static int clamp_int(int value, int low, int high)
{
    if (value < low)
        return low;
    if (value > high)
        return high;
    return value;
}

static void keep_selection_visible(UiState *ui)
{
    if (ui->selection < 0)
        return;

    int y = plugins_row_y(ui->selection);
    int top = (int)ui->plugins_scroll;
    int visible = VIEW_H - VIEW_TOP_GAP;

    if (y < top + (int)PLUGINS_SELECT_MARGIN)
        ui->plugins_scroll = (float)(y - (int)PLUGINS_SELECT_MARGIN);
    else if (y + (int)PLUGINS_SELECT_MARGIN + (int)PLUGINS_ROW_STRIDE >
             top + visible)
        ui->plugins_scroll =
            (float)(y + (int)PLUGINS_SELECT_MARGIN +
                        (int)PLUGINS_ROW_STRIDE - visible);

    int max = plugins_max_scroll();
    if (ui->plugins_scroll < 0.0f)
        ui->plugins_scroll = 0.0f;
    if (ui->plugins_scroll > (float)max)
        ui->plugins_scroll = (float)max;
}

static void move_selection(UiState *ui, int direction)
{
    if (g_editor.row_count == 0) {
        ui->selection = -1;
        return;
    }

    int next = ui->selection < 0 ? 0 : ui->selection + direction;

    while (next >= 0 && next < g_editor.row_count &&
           !editor_row_actionable(next))
        next += direction;

    if (next < 0 || next >= g_editor.row_count)
        return;

    ui->selection = next;
    keep_selection_visible(ui);
}

static void open_toggle_modal(UiState *ui)
{
    if (ui->selection < 0)
        return;

    int mode = editor_next_action(ui->selection);
    if (mode < 0)
        return;

    const char *message = editor_confirm_text(ui->selection, mode);
    int kernel_enable =
        g_editor.rows[ui->selection].section == PLUGIN_SECTION_KERNEL &&
        mode == 1;

    if (message == NULL && !kernel_enable) {
        char err[64];
        if (editor_toggle(ui->selection, mode, err, (int)sizeof(err)) != 0)
            modal_open_error(ui, err);
        return;
    }

    char body[256];
    if (message != NULL && kernel_enable) {
        snprintf(body, sizeof(body),
                 "%s  KERNEL PLUGIN. A BAD ONE CAN STOP THE NEXT BOOT. HOLD L AT BOOT SKIPS ALL PLUGINS.",
                 message);
    } else if (message != NULL) {
        snprintf(body, sizeof(body), "%s", message);
    } else {
        snprintf(body, sizeof(body),
                 "KERNEL PLUGIN. A BAD ONE CAN STOP THE NEXT BOOT. HOLD L AT BOOT SKIPS ALL PLUGINS.");
    }

    modal_open_confirm(ui, ui->selection, mode, body);
}

static void handle_modal(UiState *ui, InputEvent event)
{
    if (event == INPUT_CANCEL) {
        ui->modal = UI_MODAL_NONE;
        return;
    }

    if (event != INPUT_ACCEPT)
        return;

    if (ui->modal == UI_MODAL_ERROR) {
        ui->modal = UI_MODAL_NONE;
        return;
    }

    int row = ui->modal_row;
    int mode = ui->modal_mode;
    ui->modal = UI_MODAL_NONE;

    char err[64];
    if (editor_toggle(row, mode, err, (int)sizeof(err)) != 0)
        modal_open_error(ui, err);
}

static InputEvent read_event(const SceCtrlData *pad, InputState *state,
                             const UiState *ui, float dt)
{
    int up = (pad->buttons & SCE_CTRL_UP) != 0 || (int)pad->ly < STICK_LOW;
    int down = (pad->buttons & SCE_CTRL_DOWN) != 0 ||
               (int)pad->ly > STICK_HIGH;
    int left = (pad->buttons & SCE_CTRL_LTRIGGER) != 0;
    int right = (pad->buttons & SCE_CTRL_RTRIGGER) != 0;
    int accept = (pad->buttons & SCE_CTRL_CROSS) != 0;
    int cancel = (pad->buttons & SCE_CTRL_CIRCLE) != 0;

    return input_update(state, up, down, left, right, accept, cancel,
                        ui->modal != UI_MODAL_NONE, dt);
}

static void handle_event(UiState *ui, InputEvent event)
{
    if (ui->modal != UI_MODAL_NONE) {
        handle_modal(ui, event);
        return;
    }

    switch (event) {
    case INPUT_TAB_LEFT:
    case INPUT_TAB_RIGHT:
        ui->tab = ui->tab == UI_TAB_HOME ? UI_TAB_PLUGINS : UI_TAB_HOME;
        if (ui->tab == UI_TAB_PLUGINS) {
            if (ui->selection < 0)
                ui->selection = editor_nearest_actionable(0);
            keep_selection_visible(ui);
        }
        break;

    case INPUT_UP:
    case INPUT_DOWN:
        if (ui->tab == UI_TAB_HOME) {
            int max = layout_max_scroll();
            ui->home_scroll += event == INPUT_UP ? -HOME_SCROLL_STEP
                                                 : HOME_SCROLL_STEP;
            ui->home_scroll = (float)clamp_int((int)ui->home_scroll, 0, max);
        } else {
            move_selection(ui, event == INPUT_UP ? -1 : 1);
        }
        break;

    case INPUT_ACCEPT:
        if (ui->tab == UI_TAB_PLUGINS)
            open_toggle_modal(ui);
        break;

    default:
        break;
    }
}

int app_run(void)
{
    (void)sceCtrlSetSamplingMode(SCE_CTRL_MODE_ANALOG);

    /*
     * Snapshot the config set before anything reads it. The editor's scan also
     * resolves the active file, so the launch set and the editable set agree.
     */
    editor_scan();
    plugins_scan();
    metrics_init();

    UiState ui;
    memset(&ui, 0, sizeof(ui));
    ui.tab = UI_TAB_HOME;
    ui.selection = editor_nearest_actionable(0);
    ui.modal = UI_MODAL_NONE;

    InputState input;
    input_reset(&input);

    uint64_t last = sceKernelGetProcessTimeWide();
    double slow_acc = 0.0;

    for (;;) {
        uint64_t now = sceKernelGetProcessTimeWide();
        float dt = (float)((double)(now - last) / MICROS_PER_SECOND);
        last = now;

        if (dt > MAX_FRAME_DT)
            dt = MAX_FRAME_DT;

        SceCtrlData pad;
        memset(&pad, 0, sizeof(pad));

        /* A failed read leaves the pad zeroed: the screen keeps its state
         * rather than jumping, which is the safe direction (rule 6). */
        (void)sceCtrlPeekBufferPositive(0, &pad, 1);

        handle_event(&ui, read_event(&pad, &input, &ui, dt));

        metrics_sample_frame(dt);

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
        screen_draw((float)((double)now / MICROS_PER_SECOND), &ui);
        vita2d_end_drawing();
        vita2d_swap_buffers();
    }
}
