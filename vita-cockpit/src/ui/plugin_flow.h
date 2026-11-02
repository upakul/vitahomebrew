#ifndef COCKPIT_UI_PLUGIN_FLOW_H
#define COCKPIT_UI_PLUGIN_FLOW_H

#include "core/plugin_editor.h"
#include "ui/ui_state.h"

/*
 * The PLUGINS dialog copy and the session action order. This lives outside
 * app.c so the host preview can open the same dialogs the app opens instead of
 * maintaining a second copy of the text (D26).
 */

typedef enum {
    PLUGIN_FLOW_REBOOT = 0,
    PLUGIN_FLOW_UNDO,
    PLUGIN_FLOW_CLOSE,
} PluginFlowAction;

void plugin_flow_confirm(UiState *ui, int row, int mode);
void plugin_flow_receipt(UiState *ui, int row, int mode);
void plugin_flow_restored(UiState *ui);
void plugin_flow_error(UiState *ui, int row, int mode,
                       EditorWriteResult result, const char *err);
void plugin_flow_undo_error(UiState *ui, EditorWriteResult result,
                            const char *err);
void plugin_flow_session(UiState *ui);

int plugin_flow_session_count(void);
PluginFlowAction plugin_flow_session_at(int index);
const char *plugin_flow_session_label(PluginFlowAction action);

/* A refusal message for the restart gate, or NULL when reboot is allowed. */
const char *plugin_flow_restart_blocked(void);
void plugin_flow_restart(UiState *ui);

#endif
