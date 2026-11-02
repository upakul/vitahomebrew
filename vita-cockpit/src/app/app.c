#include "app/app.h"

#include <psp2/ctrl.h>
#include <psp2/kernel/processmgr.h>
#include <stdio.h>
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
#include "ui/ui_state.h"

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
#define PLUGINS_SCROLL_STEP 40.0f

#define REBOOT_MIN_PERCENT 20
#define MODAL_TITLE_LABEL_MAX 34
#define TEXT_MAX 512

static int clamp_int(int value, int low, int high)
{
    if (value < low)
        return low;
    if (value > high)
        return high;
    return value;
}

static void append_text(char *dst, size_t cap, const char *text)
{
    size_t used = strlen(dst);

    if (text == NULL || text[0] == '\0' || used + 2 >= cap)
        return;

    if (used > 0) {
        dst[used++] = ' ';
        dst[used] = '\0';
    }

    snprintf(dst + used, cap - used, "%s", text);
}

/* A title that fits the 552 px modal content width at label scale. */
static void fit_title(char *dst, size_t cap, const char *verb,
                      const char *label)
{
    int len = (int)strlen(label);

    if (len > MODAL_TITLE_LABEL_MAX)
        snprintf(dst, cap, "%s %.34s..", verb, label);
    else
        snprintf(dst, cap, "%s %s", verb, label);
}

static void set_action_title(UiState *ui, const char *verb, const char *label)
{
    fit_title(ui->modal_title, sizeof(ui->modal_title), verb, label);
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

static void compose_confirm_body(char *body, size_t cap, int row, int mode)
{
    const EditorRow *r = &g_editor.rows[row];
    const char *note = editor_confirm_text(row, mode);
    char count[96];

    body[0] = '\0';
    append_text(body, cap, "WRITTEN NOW. APPLIES AT THE NEXT BOOT.");

    if (note != NULL)
        append_text(body, cap, note);

    if (r->section == PLUGIN_SECTION_KERNEL) {
        append_text(body, cap,
                    "KERNEL PLUGIN. IF IT IS BAD THE VITA MAY NOT BOOT. HOLD L "
                    "FROM POWER-ON TO SKIP CONFIG.TXT PLUGINS.");
    } else if (r->section == PLUGIN_SECTION_SYSTEM) {
        append_text(body, cap,
                    "SYSTEM SHELL PLUGIN. A BAD ONE CAN CRASH LIVEAREA.");
    }

    if (r->line_count > 1) {
        snprintf(count, sizeof(count),
                 "THIS FILE IS LISTED %d TIMES. ALL REACHED LINES CHANGE.",
                 r->line_count);
        append_text(body, cap, count);
    }

    if (g_editor.pending_count > 0) {
        snprintf(count, sizeof(count), "THIS IS CHANGE %d THIS SESSION.",
                 g_editor.pending_count + 1);
        append_text(body, cap, count);
    }
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
    if (mode < 0) {
        const char *reason = editor_action_reason(ui->selection);
        modal_open_info(ui, reason != NULL ? reason : "COCKPIT CANNOT CHANGE THIS ROW.");
        return;
    }

    char body[TEXT_MAX];
    compose_confirm_body(body, sizeof(body), ui->selection, mode);
    modal_open_confirm(ui, ui->selection, mode, body);
    set_action_title(ui, mode == EDITOR_ACTION_ENABLE ? "ENABLE" : "DISABLE",
                     editor_row_label(ui->selection));
}

static void open_receipt(UiState *ui, int row, int mode)
{
    char body[TEXT_MAX];
    char title[64];
    const char *label = editor_row_label(row);
    char pending[96];

    snprintf(body, sizeof(body),
             "WRITTEN TO %s. APPLIES AT THE NEXT BOOT. THE CHANGE IS SAVED - "
             "LEAVING COCKPIT DOES NOT CANCEL IT.",
             g_editor.source);

    if (g_editor.pending_count > 0) {
        snprintf(pending, sizeof(pending),
                 "%d CHANGES WAIT FOR THE NEXT BOOT.",
                 g_editor.pending_count);
        append_text(body, sizeof(body), pending);
    }

    fit_title(title, sizeof(title),
              mode == EDITOR_ACTION_ENABLE ? "ENABLED" : "DISABLED", label);
    modal_open_receipt(ui, title, body);
}

static void open_toggle_error(UiState *ui, int row, int mode, const char *err)
{
    char body[TEXT_MAX];
    const char *label = editor_row_label(row);

    if (strstr(err, "RESTORE") != NULL) {
        snprintf(body, sizeof(body),
                 "%s FAILED AND THE ORIGINAL COULD NOT BE PUT BACK. THE CONFIG "
                 "MAY BE INCOMPLETE. DO NOT REBOOT. CHECK CONFIG.TXT IN "
                 "VITASHELL.",
                 mode == EDITOR_ACTION_ENABLE ? "ENABLE" : "DISABLE");
    } else {
        snprintf(body, sizeof(body),
                 "%s %s FAILED. %s. THE CONFIG IS UNCHANGED. TRY AGAIN, OR "
                 "CHECK THE TAI FOLDER IN VITASHELL.",
                 mode == EDITOR_ACTION_ENABLE ? "ENABLE" : "DISABLE", label,
                 err);
    }

    modal_open_error(ui, body);
}

static void do_toggle(UiState *ui, int row, int mode)
{
    char err[96];

    if (editor_toggle(row, mode, err, (int)sizeof(err)) != 0) {
        open_toggle_error(ui, row, mode, err);
        return;
    }

    if (ui->selection >= g_editor.row_count)
        ui->selection = g_editor.row_count > 0 ? g_editor.row_count - 1 : -1;

    open_receipt(ui, row, mode);
}

static void do_undo(UiState *ui)
{
    char err[96];

    if (editor_undo(err, (int)sizeof(err)) != 0) {
        modal_open_error(ui, err);
        return;
    }

    if (ui->selection >= g_editor.row_count)
        ui->selection = g_editor.row_count > 0 ? g_editor.row_count - 1 : -1;

    modal_open_receipt(ui, "RESTORED",
                       "THE CONFIG IS BACK TO BEFORE THE LAST CHANGE. THE "
                       "RESTORED FILE IS THE NEW SESSION START. IF THE VITA HAS "
                       "NOT RESTARTED, THE RUNNING PLUGINS ARE UNCHANGED.");
}

static void open_restart(UiState *ui)
{
    char body[TEXT_MAX];

    if (!g_metrics.power_valid) {
        modal_open_info(ui,
                        "THE BATTERY LEVEL IS UNKNOWN. CHARGE THE VITA BEFORE "
                        "REBOOTING, THEN TRY AGAIN.");
        snprintf(ui->modal_title, sizeof(ui->modal_title), "REBOOT BLOCKED");
        return;
    }

    if (g_metrics.batt_pct < REBOOT_MIN_PERCENT && !g_metrics.charging) {
        modal_open_info(ui,
                        "CHARGE TO 20 PERCENT BEFORE REBOOTING. A REBOOT WHILE "
                        "THE CARD IS BEING WRITTEN CAN LEAVE IT UNREADABLE.");
        snprintf(ui->modal_title, sizeof(ui->modal_title), "REBOOT BLOCKED");
        return;
    }

    snprintf(body, sizeof(body),
             "THE CONSOLE WILL RESTART NOW. %d CHANGES WILL APPLY AT BOOT. DO "
             "NOT REMOVE THE MEMORY CARD.",
             g_editor.pending_count);
    modal_open_restart(ui, body);
}

static void do_restart(UiState *ui)
{
    int rc = platform_request_reboot();

    if (rc < 0) {
        modal_open_error(ui,
                         "RESTART FAILED. TURN THE VITA OFF AND ON BY HAND. "
                         "YOUR CHANGES ARE SAVED.");
        snprintf(ui->modal_title, sizeof(ui->modal_title), "NOT RESTARTED");
        return;
    }

    /*
     * A 0 return means the request was accepted, not that the reset happened.
     * If this frame still runs, say what to do rather than leave the screen
     * frozen on a promise.
     */
    modal_open_info(ui,
                    "REBOOT REQUESTED. IF THE VITA DOES NOT RESTART IN A FEW "
                    "SECONDS, HOLD POWER FOR 5 SECONDS AND TURN IT ON AGAIN.");
    snprintf(ui->modal_title, sizeof(ui->modal_title), "REBOOT");
}

static void open_session(UiState *ui)
{
    const char *choices[UI_MODAL_CHOICES_MAX];
    int count = 0;
    char body[TEXT_MAX];

    if (g_editor.pending_count > 0)
        choices[count++] = "REBOOT NOW";
    if (g_editor.undo_available)
        choices[count++] = "UNDO LAST CHANGE";
    choices[count++] = "CLOSE";

    snprintf(body, sizeof(body), "%d LOADED   %d NEXT BOOT.",
             g_editor.activated_total, g_editor.pending_count);

    if (g_editor.undo_available)
        append_text(body, sizeof(body), "AN UNDO SNAPSHOT IS AVAILABLE.");

    modal_open_session(ui, body, choices, count);
}

static void choose_session(UiState *ui)
{
    int reboot_index = g_editor.pending_count > 0 ? 0 : -1;
    int undo_index =
        g_editor.undo_available ? (g_editor.pending_count > 0 ? 1 : 0) : -1;

    if (ui->modal_choice == reboot_index) {
        ui->modal = UI_MODAL_NONE;
        open_restart(ui);
    } else if (ui->modal_choice == undo_index) {
        ui->modal = UI_MODAL_NONE;
        modal_open_confirm(ui, -1, 2,
                           "RESTORE THE CONFIG FROM BEFORE THE LAST CHANGE? "
                           "THIS UNDOES ONE CHANGE. IT DOES NOT TOUCH "
                           "BOOT_CONFIG.TXT.");
        snprintf(ui->modal_title, sizeof(ui->modal_title), "UNDO LAST CHANGE");
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
                 ui->modal_choice + 1 < ui->modal_choice_count)
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
        ui->modal = UI_MODAL_NONE;
        do_restart(ui);
        return;
    }

    int row = ui->modal_row;
    int mode = ui->modal_mode;
    ui->modal = UI_MODAL_NONE;

    if (mode == 2)
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

    return input_update(state, up, down, left, right, accept, cancel, options,
                        ui->modal != UI_MODAL_NONE,
                        ui->modal == UI_MODAL_SESSION, dt);
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
            open_session(ui);
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
