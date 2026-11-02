#ifndef COCKPIT_CORE_PLUGIN_EDITOR_H
#define COCKPIT_CORE_PLUGIN_EDITOR_H

#include "core/plugins.h"

/*
 * The editable view of the active taiHEN config (D55, D56, D61-D66).
 *
 * core/plugins.c owns the launch-activated set used by HOME. This module owns
 * the full line model used by PLUGINS: disabled entries, sections, `*!` halts,
 * the two snapshots, pending state, and the toggle plan. Keeping them separate
 * is what stops an edit from changing the HOME count before the reboot.
 */

#define EDITOR_TEXT_MAX 32768
#define EDITOR_LINES_MAX 512
#define EDITOR_ROWS_MAX EDITOR_LINES_MAX
#define EDITOR_ORPHANS_MAX 32

typedef enum {
    EDITOR_LINE_BLANK = 0,
    EDITOR_LINE_COMMENT,
    EDITOR_LINE_HEADER,
    EDITOR_LINE_PATH,
} EditorLineKind;

typedef enum {
    EDITOR_ACTIVATED = 0,
    EDITOR_DISABLED,
    EDITOR_MISSING,
    EDITOR_PARTIAL,
    EDITOR_NOT_REACHED,
    EDITOR_BOOT,
} EditorStatus;

typedef struct {
    int start;       /* first byte of the line */
    int len;         /* bytes before the line ending */
    int content;     /* first non-space byte, after a BOM */
    int content_len; /* trimmed content length */
    int hash;        /* first '#', or -1 */
    int hash_count;  /* number of leading '#' markers */
    int path_start;  /* first byte of the plugin path */
    int newline;     /* 1 for LF or CR, 2 for CRLF */
    int kind;
    int active;
    int reached;
    int launch_active;
    PluginSection section;
    char path[PLUGIN_PATH_MAX];
    int row; /* index into rows, or -1 */
} EditorLine;

typedef struct {
    char path[PLUGIN_PATH_MAX];
    char name[PLUGIN_NAME_MAX];
    char desc[PLUGIN_DESC_MAX];
    PluginSection section;
    EditorStatus status;
    int line_count;
    int active_lines;
    int reached_lines;
    int first_line;
    int locked;
    int boot;
    int pending;
} EditorRow;

typedef struct {
    char text[EDITOR_TEXT_MAX];
    int text_len;

    char last_text[EDITOR_TEXT_MAX];
    int last_len;

    char source[PLUGIN_PATH_MAX];
    char ignored[PLUGIN_PATH_MAX];
    int source_ok;
    int valid;        /* no path before the first section, no bad path */
    int read_only;
    int too_large;
    int invalid;      /* warning only: a path before the first section */
    int unreadable;
    int orphans_unverified;
    int truncated;        /* rows were dropped by the row ceiling */
    int rows_truncated;
    int lines_truncated;
    int backup_warning;
    int backup_original_ok;
    int backup_session_ok;

    EditorLine lines[EDITOR_LINES_MAX];
    int line_count;

    EditorRow rows[EDITOR_ROWS_MAX];
    int row_count;

    int pending_count;
    int activated_total;
    int group_counts[PLUGIN_SECTION_COUNT];

    char orphans[EDITOR_ORPHANS_MAX][PLUGIN_PATH_MAX];
    int orphan_shadowed[EDITOR_ORPHANS_MAX];
    int orphan_count;
    int orphan_truncated;
} EditorState;

extern EditorState g_editor;

/* Snapshot the config set, resolve the active file, parse it, and build rows. */
void editor_scan(void);

/*
 * Apply the next toggle to a row. mode is 0 disable, 1 enable. Returns 0 on
 * success; on failure `err` carries a short, font-safe message.
 */
int editor_toggle(int row, int mode, char *err, int err_cap);

/* 0 disable, 1 enable, -1 no action. */
int editor_next_action(int row);

int editor_row_actionable(int row);
const char *editor_status_text(EditorStatus status);
const char *editor_section_text(PluginSection section);

/* The label for the row's action control: ENABLE, DISABLE, LOCKED, ... */
const char *editor_action_text(int row);

/* Why the control is greyed, or NULL when the action is possible. */
const char *editor_action_reason(int row);

/* Why the whole page is read-only, or NULL when editing is possible. */
const char *editor_readonly_reason(void);

/* A warning message for the row and mode, or NULL when no confirmation is due. */
const char *editor_confirm_text(int row, int mode);

int editor_row_for_path(const char *path);

#endif
