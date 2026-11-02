#include "ui/plugin_flow.h"

#include <stdio.h>
#include <string.h>

#include "core/metrics.h"
#include "core/plugin_editor.h"
#include "ui/modal.h"

#define FLOW_TEXT_MAX 512
#define FLOW_COUNT_MAX 96
#define REBOOT_MIN_PERCENT 20

static const char *action_verb(int mode)
{
    return mode == EDITOR_ACTION_ENABLE ? "ENABLE" : "DISABLE";
}

static const char *action_done(int mode)
{
    return mode == EDITOR_ACTION_ENABLE ? "ENABLED" : "DISABLED";
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

static void fit_title(char *dst, size_t cap, const char *verb,
                      const char *label)
{
    int len = (int)strlen(label);

    if (len > UI_MODAL_TITLE_TEXT_MAX)
        snprintf(dst, cap, "%s %.*s..", verb, UI_MODAL_TITLE_TEXT_MAX, label);
    else
        snprintf(dst, cap, "%s %s", verb, label);
}

void plugin_flow_confirm(UiState *ui, int row, int mode)
{
    if (ui == NULL || row < 0 || row >= g_editor.row_count)
        return;

    const EditorRow *r = &g_editor.rows[row];
    const char *note = editor_confirm_text(row, mode);
    char body[FLOW_TEXT_MAX];
    char count[FLOW_COUNT_MAX];
    char title[64];

    body[0] = '\0';
    append_text(body, sizeof(body), "WRITTEN NOW. APPLIES AT THE NEXT BOOT.");

    if (note != NULL)
        append_text(body, sizeof(body), note);

    if (r->section == PLUGIN_SECTION_KERNEL) {
        append_text(body, sizeof(body),
                    "KERNEL PLUGIN. IF IT IS BAD THE VITA MAY NOT BOOT. HOLD L "
                    "FROM POWER-ON TO SKIP CONFIG.TXT PLUGINS.");
    } else if (r->section == PLUGIN_SECTION_SYSTEM) {
        append_text(body, sizeof(body),
                    "SYSTEM SHELL PLUGIN. A BAD ONE CAN CRASH LIVEAREA.");
    }

    if (r->line_count > 1) {
        snprintf(count, sizeof(count),
                 "THIS FILE IS LISTED %d TIMES. ALL REACHED LINES CHANGE.",
                 r->line_count);
        append_text(body, sizeof(body), count);
    }

    if (g_editor.pending_count > 0) {
        snprintf(count, sizeof(count), "THIS IS CHANGE %d THIS SESSION.",
                 g_editor.pending_count + 1);
        append_text(body, sizeof(body), count);
    }

    fit_title(title, sizeof(title), action_verb(mode),
              editor_row_label(row));
    modal_open_confirm_titled(ui, title, row, mode, body);
}

void plugin_flow_receipt(UiState *ui, int row, int mode)
{
    if (ui == NULL || row < 0 || row >= g_editor.row_count)
        return;

    char body[FLOW_TEXT_MAX];
    char title[64];
    char pending[FLOW_COUNT_MAX];

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

    fit_title(title, sizeof(title), action_done(mode),
              editor_row_label(row));
    modal_open_receipt(ui, title, body);
}

void plugin_flow_restored(UiState *ui)
{
    if (ui == NULL)
        return;

    modal_open_receipt(ui, "RESTORED",
                       "THE CONFIG IS BACK TO BEFORE THE LAST CHANGE. THE "
                       "RESTORED FILE IS THE NEW SESSION START. IF THE VITA HAS "
                       "NOT RESTARTED, THE RUNNING PLUGINS ARE UNCHANGED.");
}

void plugin_flow_error(UiState *ui, int row, int mode,
                       EditorWriteResult result, const char *err){
    if (ui == NULL)
        return;

    char body[FLOW_TEXT_MAX];

    if (row < 0 || row >= g_editor.row_count) {
        plugin_flow_undo_error(ui, result, err);
        return;
    }

    if (result == EDITOR_WRITE_UNRECOVERED) {
        snprintf(body, sizeof(body),
                 "THE CHANGE FAILED AND THE ORIGINAL COULD NOT BE PUT BACK. "
                 "THE CONFIG MAY BE INCOMPLETE. DO NOT REBOOT. CHECK "
                 "CONFIG.TXT IN VITASHELL.");
        modal_open_error_titled(ui, "DO NOT REBOOT", body);
        return;
    }

    snprintf(body, sizeof(body),
             "%s %s FAILED. %s. THE CONFIG IS UNCHANGED. TRY AGAIN, OR CHECK "
             "THE TAI FOLDER IN VITASHELL.",
             action_verb(mode), editor_row_label(row), err);
    modal_open_error_titled(ui, "NOT WRITTEN", body);
}

void plugin_flow_undo_error(UiState *ui, EditorWriteResult result,
                            const char *err)
{
    if (ui == NULL)
        return;

    char body[FLOW_TEXT_MAX];

    if (result == EDITOR_WRITE_UNRECOVERED) {
        snprintf(body, sizeof(body),
                 "THE RESTORE FAILED AND THE ORIGINAL COULD NOT BE PUT BACK. "
                 "THE CONFIG MAY BE INCOMPLETE. DO NOT REBOOT. CHECK "
                 "CONFIG.TXT IN VITASHELL.");
        modal_open_error_titled(ui, "DO NOT REBOOT", body);
        return;
    }

    snprintf(body, sizeof(body), "%s. NOTHING RESTORED.", err);
    modal_open_error_titled(ui, "NOT WRITTEN", body);
}

int plugin_flow_session_count(void)
{
    for (int i = 0; i < UI_MODAL_CHOICES_MAX; ++i) {
        if (plugin_flow_session_at(i) == PLUGIN_FLOW_CLOSE)
            return i + 1;
    }

    return 1;
}

PluginFlowAction plugin_flow_session_at(int index)
{
    int cursor = 0;

    if (g_editor.pending_count > 0) {
        if (index == cursor)
            return PLUGIN_FLOW_REBOOT;
        ++cursor;
    }

    if (g_editor.undo_available) {
        if (index == cursor)
            return PLUGIN_FLOW_UNDO;
    }

    return PLUGIN_FLOW_CLOSE;
}

const char *plugin_flow_session_label(PluginFlowAction action)
{
    switch (action) {
    case PLUGIN_FLOW_REBOOT:
        return "REBOOT NOW";
    case PLUGIN_FLOW_UNDO:
        return "UNDO LAST CHANGE";
    case PLUGIN_FLOW_CLOSE:
    default:
        return "CLOSE";
    }
}

void plugin_flow_session(UiState *ui)
{
    if (ui == NULL)
        return;

    const char *choices[UI_MODAL_CHOICES_MAX];
    int count = plugin_flow_session_count();
    char body[FLOW_TEXT_MAX];

    for (int i = 0; i < count; ++i)
        choices[i] = plugin_flow_session_label(plugin_flow_session_at(i));

    snprintf(body, sizeof(body), "%d LOADED   %d NEXT BOOT.",
             g_editor.loaded_total, g_editor.pending_count);

    if (g_editor.undo_available)
        append_text(body, sizeof(body), "AN UNDO SNAPSHOT IS AVAILABLE.");

    modal_open_session(ui, body, choices, count);
}

const char *plugin_flow_restart_blocked(void)
{
    if (!g_metrics.power_valid)
        return "THE BATTERY LEVEL IS UNKNOWN. CHARGE THE VITA BEFORE "
               "REBOOTING, THEN TRY AGAIN.";

    if (g_metrics.batt_pct < REBOOT_MIN_PERCENT && !g_metrics.charging) {
        static char msg[160];

        snprintf(msg, sizeof(msg),
                 "CHARGE TO %d PERCENT BEFORE REBOOTING. A REBOOT WHILE THE "
                 "CARD IS BEING WRITTEN CAN LEAVE IT UNREADABLE.",
                 REBOOT_MIN_PERCENT);
        return msg;
    }

    return NULL;
}

void plugin_flow_restart(UiState *ui)
{
    if (ui == NULL)
        return;

    char body[FLOW_TEXT_MAX];

    snprintf(body, sizeof(body),
             "THE CONSOLE WILL RESTART NOW. %d CHANGES WILL APPLY AT BOOT. DO "
             "NOT REMOVE THE MEMORY CARD.",
             g_editor.pending_count);
    modal_open_restart(ui, body);
}
