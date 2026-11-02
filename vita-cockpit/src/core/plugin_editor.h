#ifndef COCKPIT_CORE_PLUGIN_EDITOR_H
#define COCKPIT_CORE_PLUGIN_EDITOR_H

#include "core/plugins.h"
#include "core/tai_config.h"

/*
 * The editable view of the active taiHEN config (D55, D56, D61-D71).
 *
 * core/plugins.c owns the launch-activated set used by HOME. This module owns
 * the full line model used by PLUGINS: disabled entries, sections, `*!` halts,
 * the snapshots, pending state, and the toggle plan. Keeping them separate is
 * what stops an edit from changing the HOME count before the reboot.
 */

#define EDITOR_TEXT_MAX TAI_CONFIG_MAX
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
    EDITOR_LOADED = 0,
    EDITOR_DISABLED,
    EDITOR_MISSING,
    EDITOR_PARTIAL,
    EDITOR_NOT_REACHED,
    EDITOR_BOOT,
} EditorStatus;

typedef enum {
    EDITOR_ACTION_NONE = -1,
    EDITOR_ACTION_DISABLE = 0,
    EDITOR_ACTION_ENABLE = 1,
} EditorAction;

typedef enum {
    EDITOR_WRITE_OK = 0,
    EDITOR_WRITE_REFUSED,
    EDITOR_WRITE_FAILED,
    EDITOR_WRITE_UNRECOVERED,
} EditorWriteResult;

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
    int loaded;      /* active in the launch file: what is running now */
    int next_change; /* -1 off at boot, +1 on at boot, 0 no change */
    int duplicate_name; /* another row shares this basename */
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
    int paths_truncated; /* a path was too long and was skipped */
    int orphans_unverified;
    int truncated;        /* rows were dropped by the row ceiling */
    int rows_truncated;
    int lines_truncated;
    int backup_warning;
    int undo_available; /* a .undo snapshot differs from the live file */
    int recovered;      /* a .old file was moved back into place at scan */

    EditorLine lines[EDITOR_LINES_MAX];
    int line_count;

    EditorRow rows[EDITOR_ROWS_MAX];
    int row_count;

    int pending_count;
    int loaded_total;

    char orphans[EDITOR_ORPHANS_MAX][PLUGIN_PATH_MAX];
    int orphan_shadowed[EDITOR_ORPHANS_MAX];
    int orphan_count;
    int orphan_truncated;
} EditorState;

extern EditorState g_editor;

/* Snapshot the config set, resolve the active file, parse it, and build rows. */
void editor_scan(void);

/*
 * Apply the next toggle to a row. mode is 0 disable, 1 enable. `err` carries a
 * short, font-safe message; the result says whether the refusal happened before
 * the write or the config may be unrecovered.
 */
EditorWriteResult editor_toggle(int row, int mode, char *err, int err_cap);

/*
 * Restore the active config from `config.txt.undo`, the pre-write snapshot.
 * One step only: the snapshot is not rotated.
 */
EditorWriteResult editor_undo(char *err, int err_cap);

/* The next action for the row, or EDITOR_ACTION_NONE. */
EditorAction editor_next_action(int row);

int editor_row_actionable(int row);
const char *editor_status_text(EditorStatus status);
const char *editor_section_text(PluginSection section);

/* The basename, or the full path when two rows share a basename. */
const char *editor_row_label(int row);

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
