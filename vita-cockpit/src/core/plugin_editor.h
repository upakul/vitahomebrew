#ifndef COCKPIT_CORE_PLUGIN_EDITOR_H
#define COCKPIT_CORE_PLUGIN_EDITOR_H

#include "core/plugins.h"

/*
 * The editable view of the active taiHEN config (D55, D56, D57).
 *
 * core/plugins.c owns the launch-activated set used by HOME. This module owns
 * the full line model used by PLUGINS: disabled entries, sections, `*!` halts,
 * the backup set, pending state, and the toggle plan. Keeping them separate is
 * what stops an edit from changing the HOME count before the reboot.
 */

#define EDITOR_TEXT_MAX 32768
#define EDITOR_LINES_MAX 512
#define EDITOR_ROWS_MAX 64
#define EDITOR_ORPHANS_MAX 12
#define EDITOR_BOOT_MAX 12

typedef enum {
    EDITOR_LINE_BLANK = 0,
    EDITOR_LINE_COMMENT,
    EDITOR_LINE_HEADER,
    EDITOR_LINE_PATH,
} EditorLineKind;

typedef enum {
    EDITOR_ENABLED = 0,
    EDITOR_DISABLED,
    EDITOR_MISSING,
    EDITOR_PARTIAL,
    EDITOR_NOT_REACHED,
    EDITOR_BOOT,
} EditorStatus;

typedef struct {
    int start;       /* first byte of the line */
    int len;         /* bytes before the newline */
    int content;     /* first non-space byte */
    int content_len; /* trimmed content length */
    int newline;     /* 1 for LF, 2 for CRLF */
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
    char launch_text[EDITOR_TEXT_MAX];
    int launch_len;
    char last_text[EDITOR_TEXT_MAX];
    int last_len;

    char source[PLUGIN_PATH_MAX];
    char ignored[PLUGIN_PATH_MAX];
    int source_ok;
    int valid;
    int read_only;
    int too_large;
    int invalid;
    int boot_ok;
    int orphans_unverified;
    int truncated;
    int lines_truncated;
    int backup_warning;

    EditorLine lines[EDITOR_LINES_MAX];
    int line_count;

    EditorRow rows[EDITOR_ROWS_MAX];
    int row_count;

    int pending_count;
    int activated_total;
    int group_counts[PLUGIN_SECTION_COUNT];

    char boot_paths[EDITOR_BOOT_MAX][PLUGIN_PATH_MAX];
    int boot_count;

    char orphans[EDITOR_ORPHANS_MAX][PLUGIN_PATH_MAX];
    int orphan_shadowed[EDITOR_ORPHANS_MAX];
    int orphan_count;
} EditorState;

extern EditorState g_editor;

/* Snapshot the config set, resolve the active file, parse it, and build rows. */
void editor_scan(void);

/*
 * Apply the next toggle to a row. mode is 0 disable, 1 enable, 2 restore the
 * launch pattern. Returns 0 on success; on failure `err` carries a short,
 * font-safe message.
 */
int editor_toggle(int row, int mode, char *err, int err_cap);

/* 0 disable, 1 enable, 2 restore, -1 no action. */
int editor_next_action(int row);

int editor_row_actionable(int row);
const char *editor_status_text(EditorStatus status);
const char *editor_section_text(PluginSection section);

/* A warning message for the row and mode, or NULL when no confirmation is due. */
const char *editor_confirm_text(int row, int mode);

int editor_row_for_path(const char *path);

/* Nearest actionable row at or after `from`; -1 when none exists. */
int editor_nearest_actionable(int from);

#endif
