#include "app/app.h"

#include <psp2/ctrl.h>
#include <psp2/kernel/processmgr.h>
#include <string.h>
#include <vita2d.h>

#include "app/screen.h"
#include "core/input.h"
#include "core/metrics.h"
#include "core/platform.h"
#include "core/plugin_editor.h"
#include "core/plugins.h"
#include "gfx/layout.h"
#include "panels/plugins.h"
#include "panels/registry.h"
#include "ui/modal.h"
#include "ui/plugin_flow.h"
#include "ui/ui_state.h"

/*
 * Lifecycle, time, scheduling, and input. This file owns the frame loop and
 * the one mutable UiState. What is drawn lives in screen.c; how anything looks
 * lives in gfx/theme; dialog copy lives in ui/plugin_flow.
 */

#define MICROS_PER_SECOND 1000000.0

/* Analog dead zones for the list cursor. */
#define STICK_LOW 70
#define STICK_HIGH 185

#define HOME_SCROLL_STEP 60.0f
#define PLUGINS_SCROLL_STEP 40.0f

static int clamp_int(int value, int low, int high)
{
    if (value < low)
        return low;
    if (value > high)
        return high;
    return value;
}

static void keep_selection_valid(UiState *ui)
{
    if (ui->selection >= g_editor.row_count)
        ui->selection = g_editor.row_count > 0 ? g_editor.row_count - 1 : -1;
}

static void keep_selection_visible(UiState *ui)
{
    if (ui->selection < 0 || ui->selection >= g_editor.row_count)
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

    /*
     * Every row is selectable, including a locked or missing one. The old loop
     * skipped non-actionable rows, so the user could not even put the cursor on
     * a missing plugin to read why its control was greyed.
     */
    int next = ui->selection < 0 ? 0 : ui->selection + direction;

    if (next < 0)
        next = 0;
    if (next >= g_editor.row_count)
        next = g_editor.row_count - 1;

    ui->selection = next;
    keep_selection_visible(ui);
}

static void scroll_plugins(UiState *ui, float delta)
{
    ui->plugins_scroll += delta;

    int max = plugins_max_scroll();
    if (ui->plugins_scroll < 0.0f)
        ui->plugins_scroll = 0.0f;
    if (ui->plugins_scroll > (float)max)
        ui->plugins_scroll = (float)max;
}

static int selection_visible(const UiState *ui)
{
    if (ui->selection < 0 || ui->selection >= g_editor.row_count)
        return 0;

    int y = plugins_row_y(ui->selection);
    int screen_y =
        VIEW_Y + VIEW_TOP_GAP - (int)ui->plugins_scroll + y;

    return screen_y + (int)PLUGINS_ROW_STRIDE > VIEW_Y &&
           screen_y < VIEW_Y + VIEW_H;
}

static void open_toggle_modal(UiState *ui)
{
    if (ui->selection < 0 || ui->selection >= g_editor.row_count)
        return;

    /*
     * Scrolling past the last row reveals the orphan section. Cross must not
     * act on a row whose control is no longer on screen; bring it back first.
     */
    if (!selection_visible(ui)) {
        keep_selection_visible(ui);
        return;
    }

    int mode = editor_next_action(ui->selection);
    if (mode == EDITOR_ACTION_NONE) {
        const char *reason = editor_action_reason(ui->selection);

        modal_open_info(ui, reason != NULL ? reason
                                           : "COCKPIT CANNOT CHANGE THIS ROW.");
        return;
    }

    plugin_flow_confirm(ui, ui->selection, mode);
}

static void do_toggle(UiState *ui, int row, int mode)
{
    char err[96];
    EditorWriteResult result = editor_toggle(row, mode, err, (int)sizeof(err));

    if (result != EDITOR_WRITE_OK) {
        plugin_flow_error(ui, row, mode, result, err);
        return;
    }

    keep_selection_valid(ui);
    plugin_flow_receipt(ui, row, mode);
}

static void do_undo(UiState *ui)
{
    char err[96];
    EditorWriteResult result = editor_undo(err, (int)sizeof(err));

    if (result != EDITOR_WRITE_OK) {
        plugin_flow_undo_error(ui, result, err);
        return;
    }

    keep_selection_valid(ui);
    plugin_flow_restored(ui);
}

static void do_restart(UiState *ui)
{
    int rc = platform_request_reboot();

    if (rc < 0) {
        modal_open_error_titled(ui, "NOT RESTARTED",
                                "RESTART FAILED. TURN THE VITA OFF AND ON BY "
                                "HAND. YOUR CHANGES ARE SAVED.");
        return;
    }

    /*
     * A 0 return means the request was accepted, not that the reset happened.
     * If this frame still runs, say what to do rather than leave the screen
     * frozen on a promise.
     */
    modal_open_info_titled(ui, "REBOOT",
                           "REBOOT REQUESTED. IF THE VITA DOES NOT RESTART IN A "
                           "FEW SECONDS, HOLD POWER FOR 5 SECONDS AND TURN IT "
                           "ON AGAIN.");
}

static void choose_session(UiState *ui)
{
    PluginFlowAction action = plugin_flow_session_at(ui->modal_choice);

    if (action == PLUGIN_FLOW_REBOOT) {
        const char *blocked = plugin_flow_restart_blocked();

        if (blocked != NULL)
            modal_open_info_titled(ui, "REBOOT BLOCKED", blocked);
        else
            plugin_flow_restart(ui);
    } else if (action == PLUGIN_FLOW_UNDO) {
        modal_open_confirm_titled(
            ui, "UNDO LAST CHANGE", -1, UI_MODAL_MODE_RESTORE,
            "RESTORE THE CONFIG FROM BEFORE THE LAST CHANGE? THIS UNDOES ONE "
            "CHANGE. IT DOES NOT TOUCH BOOT_CONFIG.TXT.");
    } else {
        ui->modal = UI_MODAL_NONE;
    }
}

static void handle_modal(UiState *ui, InputEvent event)
{
    if (event == INPUT_CANCEL) {
        ui->modal = UI_MODAL_NONE;
        return;
    }

    if (ui->modal == UI_MODAL_SESSION) {
        if (event == INPUT_UP && ui->modal_choice > 0)
            --ui->modal_choice;
        else if (event == INPUT_DOWN &&
                 ui->modal_choice + 1 < plugin_flow_session_count())
            ++ui->modal_choice;
        else if (event == INPUT_ACCEPT)
            choose_session(ui);
        return;
    }

    if (event != INPUT_ACCEPT)
        return;

    if (ui->modal == UI_MODAL_ERROR || ui->modal == UI_MODAL_INFO ||
        ui->modal == UI_MODAL_RECEIPT) {
        ui->modal = UI_MODAL_NONE;
        return;
    }

    if (ui->modal == UI_MODAL_RESTART) {
        do_restart(ui);
        return;
    }

    int row = ui->modal_row;
    int mode = ui->modal_mode;

    if (mode == UI_MODAL_MODE_RESTORE)
        do_undo(ui);
    else
        do_toggle(ui, row, mode);
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
    int options = (pad->buttons & SCE_CTRL_TRIANGLE) != 0;
    InputContext context = INPUT_CONTEXT_APP;

    if (ui->modal == UI_MODAL_SESSION)
        context = INPUT_CONTEXT_MODAL_LIST;
    else if (ui->modal != UI_MODAL_NONE)
        context = INPUT_CONTEXT_MODAL;

    return input_update(state, up, down, left, right, accept, cancel, options,
                        context, dt);
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
            if (ui->selection < 0 && g_editor.row_count > 0)
                ui->selection = 0;
            keep_selection_visible(ui);
        }
        break;

    case INPUT_OPTIONS:
        if (ui->tab == UI_TAB_PLUGINS)
            plugin_flow_session(ui);
        break;

    case INPUT_UP:
    case INPUT_DOWN:
        if (ui->tab == UI_TAB_HOME) {
            int max = layout_max_scroll();
            ui->home_scroll += event == INPUT_UP ? -HOME_SCROLL_STEP
                                                 : HOME_SCROLL_STEP;
            ui->home_scroll = (float)clamp_int((int)ui->home_scroll, 0, max);
        } else if (g_editor.row_count == 0 ||
                   (event == INPUT_DOWN &&
                    ui->selection >= g_editor.row_count - 1) ||
                   (event == INPUT_UP && ui->selection <= 0)) {
            /*
             * At the end of the list the cursor stops, but the orphan section
             * can be below the last row. A further press scrolls the canvas so
             * the installed-but-unlisted files stay reachable.
             */
            scroll_plugins(ui, event == INPUT_UP ? -PLUGINS_SCROLL_STEP
                                                 : PLUGINS_SCROLL_STEP);
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
    ui.selection = g_editor.row_count > 0 ? 0 : -1;
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
