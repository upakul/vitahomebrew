#include "core/plugin_editor.h"

#include <stdio.h>
#include <string.h>

#include "core/platform.h"
#include "core/tai_config.h"

EditorState g_editor;

/*
 * Three shared 32 KiB scratch buffers. The editor never needs more than these
 * at once: a scan read, a work buffer (toggle output / undo live check), and
 * the undo snapshot. Keeping them file-scope avoids seven separate statics
 * (~224 KiB of BSS) without putting them on the stack.
 */
static char s_scan[EDITOR_TEXT_MAX];
static char s_work[EDITOR_TEXT_MAX];
static char s_undo[EDITOR_TEXT_MAX];

static int matches_last_text(const char *buf, int len)
{
    return len == g_editor.last_len &&
           memcmp(buf, g_editor.last_text, (size_t)len) == 0;
}

static const char *const TAI_DIRS[2] = {TAI_DIR_UR0, TAI_DIR_UX0};

static int is_storage_driver(const char *name)
{
    return tai_ieq(name, "storagemgr.skprx") || tai_ieq(name, "usbmc.skprx") ||
           tai_ieq(name, "usbmcm.skprx");
}

static int is_boot_name(const char *name)
{
    return tai_ieq(name, "taihen.skprx") || tai_ieq(name, "henkaku.skprx") ||
           tai_ieq(name, "yamt.skprx");
}

static int is_boot_path(const EditorState *e, const char *path)
{
    (void)e;

    /*
     * boot_config.txt is deliberately not opened. The three modules below are
     * the ones documented as loading before taiHEN reads config.txt, so they
     * stay locked by name; any other boot module is outside this app's scope.
     */
    return is_boot_name(tai_base_name(path));
}

static int row_for_path(const EditorState *e, const char *path)
{
    for (int i = 0; i < e->row_count; ++i) {
        if (tai_ieq(e->rows[i].path, path))
            return i;
    }

    return -1;
}

static void rebuild_counts(EditorState *e);

static EditorStatus row_status(const EditorRow *r)
{
    if (r->boot)
        return EDITOR_BOOT;
    if (r->reached_lines == 0)
        return EDITOR_NOT_REACHED;
    if (!platform_file_exists(r->path))
        return EDITOR_MISSING;
    if (r->active_lines > 0 && r->active_lines < r->reached_lines)
        return EDITOR_PARTIAL;
    if (r->loaded)
        return EDITOR_LOADED;

    return EDITOR_DISABLED;
}

static void rebuild_rows(EditorState *e)
{
    int order[EDITOR_ROWS_MAX];

    memset(e->rows, 0, sizeof(e->rows));
    e->row_count = 0;
    e->pending_count = 0;
    e->loaded_total = 0;

    for (int i = 0; i < e->line_count; ++i) {
        EditorLine *l = &e->lines[i];

        if (l->kind != EDITOR_LINE_PATH)
            continue;

        int ri = row_for_path(e, l->path);
        if (ri < 0) {
            if (e->row_count >= EDITOR_ROWS_MAX) {
                e->truncated = 1;
                e->rows_truncated = 1;
                continue;
            }

            ri = e->row_count++;
            order[ri] = ri;

            EditorRow *r = &e->rows[ri];
            tai_copy(r->path, sizeof(r->path), l->path);
            tai_copy(r->name, sizeof(r->name), tai_base_name(l->path));
            tai_copy(r->desc, sizeof(r->desc), plugins_desc_for(r->name));
            r->section = l->section;
            r->first_line = i;
        }

        EditorRow *r = &e->rows[ri];
        if (l->section < r->section)
            r->section = l->section;

        ++r->line_count;
        if (l->reached) {
            ++r->reached_lines;
            if (l->active)
                ++r->active_lines;

            if (l->launch_active)
                r->loaded = 1;
            if (l->active != l->launch_active)
                r->next_change = 1;
        }

        l->row = ri;
    }

    for (int i = 0; i < e->row_count; ++i) {
        EditorRow *r = &e->rows[i];

        r->locked = tai_ieq(r->name, "henkaku.suprx");
        r->boot = is_boot_path(e, r->path);

        /*
         * "Loaded" is what the launch file activated, not what the file says
         * now. A write changes the next boot, never the running process; the
         * row must keep reporting the running set until the console restarts
         * (D69).
         */
        if (r->next_change != 0)
            r->next_change = r->active_lines > 0 ? 1 : -1;

        r->status = row_status(r);

        if (r->next_change != 0)
            ++e->pending_count;
    }

    /*
     * Display order is section-major, so the selection index must follow the
     * same order the panel draws. The sort is stable: rows in one section keep
     * the order their first line appeared in the file. `order` carries the
     * pre-sort row index so the line remap below is O(lines + rows), not a
     * second path search.
     */
    for (int i = 1; i < e->row_count; ++i) {
        EditorRow tmp = e->rows[i];
        int tmp_order = order[i];
        int j = i - 1;

        while (j >= 0 && e->rows[j].section > tmp.section) {
            e->rows[j + 1] = e->rows[j];
            order[j + 1] = order[j];
            --j;
        }
        e->rows[j + 1] = tmp;
        order[j + 1] = tmp_order;
    }

    int new_index[EDITOR_ROWS_MAX];

    for (int i = 0; i < e->row_count; ++i)
        new_index[order[i]] = i;

    for (int i = 0; i < e->line_count; ++i) {
        EditorLine *l = &e->lines[i];

        if (l->kind == EDITOR_LINE_PATH && l->row >= 0)
            l->row = new_index[l->row];
    }

    for (int i = 0; i < e->row_count; ++i) {
        EditorRow *r = &e->rows[i];

        for (int j = 0; j < e->row_count; ++j) {
            if (i != j && tai_ieq(r->name, e->rows[j].name)) {
                r->duplicate_name = 1;
                break;
            }
        }
    }

    rebuild_counts(e);
}

/*
 * The loaded total is counted once per row, not once per line. A row with
 * several active lines must add one. It is the launch set, so a pending edit
 * never moves it (D59, D69).
 */
static void rebuild_counts(EditorState *e)
{
    for (int i = 0; i < e->row_count; ++i) {
        if (e->rows[i].loaded)
            ++e->loaded_total;
    }
}

static void parse_text(EditorState *e, const char *text, int len, int initial,
                       const int *launch, int launch_count)
{
    if (len > EDITOR_TEXT_MAX - 1)
        len = EDITOR_TEXT_MAX - 1;

    memcpy(e->text, text, (size_t)len);
    e->text[len] = '\0';
    e->text_len = len;
    e->line_count = 0;
    e->valid = 1;
    e->invalid = 0;
    e->lines_truncated = 0;
    e->paths_truncated = 0;
    e->rows_truncated = 0;
    e->truncated = 0;

    int have_section = 0;
    int halted = 0;
    PluginSection section = PLUGIN_SECTION_TITLE;
    int i = 0;

    while (i < len && e->line_count < EDITOR_LINES_MAX) {
        int start = i;

        while (i < len && text[i] != '\n' && text[i] != '\r')
            ++i;

        int end = i;
        int newline = 0;

        if (i < len) {
            if (text[i] == '\r' && i + 1 < len && text[i + 1] == '\n') {
                i += 2;
                newline = 2;
            } else {
                ++i;
                newline = 1;
            }
        }

        int content = start;
        while (content < end && (text[content] == ' ' || text[content] == '\t'))
            ++content;

        /* A UTF-8 BOM belongs to the file, not to the first header. */
        if (start == 0 && content == 0 && tai_has_bom(text, len)) {
            content = TAI_BOM_LEN;
            while (content < end &&
                   (text[content] == ' ' || text[content] == '\t'))
                ++content;
        }

        int content_end = end;
        while (content_end > content &&
               (text[content_end - 1] == ' ' || text[content_end - 1] == '\t'))
            --content_end;

        EditorLine *l = &e->lines[e->line_count];
        memset(l, 0, sizeof(*l));
        l->start = start;
        l->len = end - start;
        l->content = content;
        l->content_len = content_end - content;
        l->hash = -1;
        l->path_start = content;
        l->newline = newline;
        l->section = section;
        l->row = -1;

        if (content == content_end) {
            l->kind = EDITOR_LINE_BLANK;
        } else if (text[content] == '*') {
            l->kind = EDITOR_LINE_HEADER;
            if (content + 1 < content_end && text[content + 1] == '!') {
                halted = 1;
                have_section = 1;
            } else {
                section = tai_section_from_header(text + content,
                                                  content_end - content);
                have_section = 1;
            }
            l->section = section;
        } else {
            /*
             * A disabled line may carry one or more '#' markers, with spaces
             * between them. The path begins after the whole comment run. This
             * makes `##foo.skprx` and `# # foo.skprx` uncomment cleanly instead
             * of leaving a '#' behind.
             */
            int p = content;
            int hash = -1;
            int hash_count = 0;

            while (p < content_end && text[p] == '#') {
                if (hash < 0)
                    hash = p;
                ++hash_count;
                ++p;
                while (p < content_end &&
                       (text[p] == ' ' || text[p] == '\t'))
                    ++p;
            }

            int path_start = p;
            int active = hash_count == 0;

            if (path_start < content_end &&
                tai_path_valid(text + path_start, content_end - path_start)) {
                if (content_end - path_start >= PLUGIN_PATH_MAX) {
                    /*
                     * One long path must not make the whole file read-only.
                     * Skip the entry and warn, exactly as the HOME parser does
                     * (D65, D71).
                     */
                    l->kind = EDITOR_LINE_COMMENT;
                    e->paths_truncated = 1;
                } else {
                    l->kind = EDITOR_LINE_PATH;
                    l->active = active;
                    l->reached = have_section && !halted;
                    l->hash = hash;
                    l->hash_count = hash_count;
                    l->path_start = path_start;

                    tai_copy_slice(l->path, sizeof(l->path), text + path_start,
                                   content_end - path_start);
                    if (!have_section)
                        e->valid = 0;
                }
            } else {
                l->kind = EDITOR_LINE_COMMENT;
            }
        }

        if (initial)
            l->launch_active = l->active;
        else if (launch != NULL && e->line_count < launch_count)
            l->launch_active = launch[e->line_count];

        ++e->line_count;
    }

    e->invalid = e->valid ? 0 : 1;

    if (i < len) {
        e->truncated = 1;
        e->lines_truncated = 1;
    }

    rebuild_rows(e);
}

typedef struct {
    EditorState *e;
    const char *dir;
} OrphanCtx;

static void orphan_cb(const char *name, int is_dir, void *user)
{
    OrphanCtx *ctx = (OrphanCtx *)user;
    EditorState *e = ctx->e;

    if (is_dir || !tai_is_plugin_path(name))
        return;
    if (tai_ends_with(name, ".bak") || tai_ends_with(name, ".new") ||
        tai_ends_with(name, ".old") || tai_ends_with(name, ".original") ||
        tai_ends_with(name, ".session") || tai_ends_with(name, ".undo"))
        return;

    if (e->orphan_count >= EDITOR_ORPHANS_MAX) {
        e->orphan_truncated = 1;
        return;
    }

    char full[PLUGIN_PATH_MAX];
    int dirlen = (int)strlen(ctx->dir);
    int namelen = (int)strlen(name);

    if (dirlen + 1 + namelen >= (int)sizeof(full)) {
        e->orphan_truncated = 1;
        return;
    }

    memcpy(full, ctx->dir, (size_t)dirlen);
    full[dirlen] = '/';
    memcpy(full + dirlen + 1, name, (size_t)namelen + 1);

    if (row_for_path(e, full) >= 0)
        return;
    if (is_boot_path(e, full))
        return;

    tai_copy(e->orphans[e->orphan_count], sizeof(e->orphans[0]), full);
    e->orphan_shadowed[e->orphan_count] =
        e->ignored[0] != '\0' &&
        strncmp(ctx->dir, e->ignored, strlen(ctx->dir)) == 0;
    ++e->orphan_count;
}

static void sort_orphans(EditorState *e)
{
    for (int i = 1; i < e->orphan_count; ++i) {
        char path[PLUGIN_PATH_MAX];
        int shadowed = e->orphan_shadowed[i];

        tai_copy(path, sizeof(path), e->orphans[i]);

        int j = i - 1;
        while (j >= 0 && strcmp(e->orphans[j], path) > 0) {
            tai_copy(e->orphans[j + 1], sizeof(e->orphans[0]), e->orphans[j]);
            e->orphan_shadowed[j + 1] = e->orphan_shadowed[j];
            --j;
        }

        tai_copy(e->orphans[j + 1], sizeof(e->orphans[0]), path);
        e->orphan_shadowed[j + 1] = shadowed;
    }
}

static void editor_refresh_undo(void)
{
    int n = 0;

    g_editor.undo_available = 0;

    if (g_editor.source[0] == '\0')
        return;

    if (platform_read_undo(g_editor.source, s_undo, (int)sizeof(s_undo), &n) !=
        PLATFORM_READ_OK)
        return;

    if (matches_last_text(s_undo, n))
        return;

    g_editor.undo_available = 1;
}

void editor_scan(void)
{
    memset(&g_editor, 0, sizeof(g_editor));

    int n = 0;

    /*
     * A crash or power loss between the two renames of the swap leaves the
     * config at `.old` and the live name empty. Recover it before deciding the
     * config is missing; otherwise the app would offer to create a new file
     * while the user's bytes sit one name away (D71).
     */
    if (platform_recover_swap(TAI_CONFIG_UX0) > 0 ||
        platform_recover_swap(TAI_CONFIG_UR0) > 0)
        g_editor.recovered = 1;

    PlatformReadStatus ux0 = platform_read_config(
        TAI_CONFIG_UX0, s_scan, (int)sizeof(s_scan), &n);
    PlatformReadStatus ur0 = PLATFORM_READ_ABSENT;

    if (ux0 == PLATFORM_READ_ABSENT) {
        ur0 = platform_read_config(TAI_CONFIG_UR0, s_scan,
                                   (int)sizeof(s_scan), &n);
    }

    TaiConfigChoice choice = tai_config_choose(ux0, ur0);
    PlatformReadStatus status = choice.status;

    if (choice.path != NULL)
        tai_copy(g_editor.source, sizeof(g_editor.source), choice.path);

    g_editor.source_ok = status == PLATFORM_READ_OK ||
                         status == PLATFORM_READ_TOO_LARGE;

    if (status == PLATFORM_READ_UNREADABLE) {
        /*
         * taiHEN tries ux0: first. If it exists but cannot be read, editing
         * ur0: would write a file the boot never loads. Name the unreadable
         * file and stop instead of silently switching mounts (D71).
         */
        g_editor.unreadable = 1;
        g_editor.read_only = 1;
    }

    int undo_probed = 0;

    if (!g_editor.source_ok) {
        g_editor.read_only = 1;
        g_editor.invalid = 1;

        /*
         * With no live config, the undo copy is the only way back. Only probe
         * when no path is already fixed: an unreadable ux0: must not be
         * replaced by a writable ur0: just because ur0: owns a snapshot.
         */
        if (g_editor.source[0] == '\0') {
            int undo_len = 0;

            if (platform_read_undo(TAI_CONFIG_UX0, s_work,
                                   (int)sizeof(s_work),
                                   &undo_len) == PLATFORM_READ_OK) {
                tai_copy(g_editor.source, sizeof(g_editor.source),
                         TAI_CONFIG_UX0);
                undo_probed = 1;
            } else if (platform_read_undo(TAI_CONFIG_UR0, s_work,
                                          (int)sizeof(s_work),
                                          &undo_len) == PLATFORM_READ_OK) {
                tai_copy(g_editor.source, sizeof(g_editor.source),
                         TAI_CONFIG_UR0);
                undo_probed = 1;
            }
        }
    } else if (status == PLATFORM_READ_TOO_LARGE) {
        g_editor.too_large = 1;
        g_editor.read_only = 1;
        g_editor.backup_warning = 1;
    } else {
        /*
         * Snapshot the bytes we just read, not a second read: the session
         * backup must be exactly the launch file even if the file changes
         * between the scan and the copy. A failed snapshot is a warning; it
         * must never lock the editor (D61).
         */
        int original_ok = 0;
        int session_ok = 0;

        if (platform_backup_config(g_editor.source, s_scan, n, &original_ok,
                                   &session_ok) != 0 ||
            !original_ok || !session_ok) {
            g_editor.backup_warning = 1;
        }

        memcpy(g_editor.last_text, s_scan, (size_t)n);
        g_editor.last_text[n] = '\0';
        g_editor.last_len = n;
        parse_text(&g_editor, s_scan, n, 1, NULL, 0);

        if (g_editor.lines_truncated || g_editor.rows_truncated)
            g_editor.read_only = 1;
    }

    if (undo_probed)
        g_editor.undo_available = 1;
    else
        editor_refresh_undo();

    if (strcmp(g_editor.source, TAI_CONFIG_UX0) == 0 &&
        platform_file_exists(TAI_CONFIG_UR0))
        tai_copy(g_editor.ignored, sizeof(g_editor.ignored), TAI_CONFIG_UR0);
    else if (strcmp(g_editor.source, TAI_CONFIG_UR0) == 0 &&
             platform_file_exists(TAI_CONFIG_UX0))
        tai_copy(g_editor.ignored, sizeof(g_editor.ignored), TAI_CONFIG_UX0);

    if (!g_editor.source_ok || g_editor.too_large || g_editor.unreadable ||
        g_editor.lines_truncated || g_editor.rows_truncated ||
        g_editor.truncated) {
        g_editor.orphans_unverified = 1;
    } else {
        for (size_t i = 0; i < sizeof(TAI_DIRS) / sizeof(TAI_DIRS[0]); ++i) {
            OrphanCtx ctx = {&g_editor, TAI_DIRS[i]};
            if (platform_list_dir(TAI_DIRS[i], orphan_cb, &ctx) < 0)
                g_editor.orphans_unverified = 1;
        }

        sort_orphans(&g_editor);
    }
}

const char *editor_readonly_reason(void)
{
    if (g_editor.unreadable)
        return "CONFIG UNREADABLE";
    if (!g_editor.source_ok)
        return "NO CONFIG.TXT";
    if (g_editor.too_large)
        return "CONFIG TOO LARGE";
    if (g_editor.lines_truncated)
        return "TOO MANY LINES";
    if (g_editor.rows_truncated)
        return "TOO MANY PLUGIN ROWS";
    if (g_editor.read_only)
        return "CONFIG IS READ ONLY";

    return NULL;
}

int editor_row_actionable(int row)
{
    return editor_next_action(row) != EDITOR_ACTION_NONE;
}

EditorAction editor_next_action(int row)
{
    if (row < 0 || row >= g_editor.row_count)
        return EDITOR_ACTION_NONE;

    if (!g_editor.source_ok || g_editor.read_only || g_editor.too_large ||
        g_editor.lines_truncated || g_editor.rows_truncated)
        return EDITOR_ACTION_NONE;

    const EditorRow *r = &g_editor.rows[row];

    if (r->status == EDITOR_BOOT || r->status == EDITOR_NOT_REACHED)
        return EDITOR_ACTION_NONE;

    /*
     * A missing file cannot be enabled. This must come before the locked rule:
     * a locked row that is also missing would otherwise advertise ENABLE and
     * then refuse it.
     */
    if (r->status == EDITOR_MISSING)
        return (r->active_lines > 0 && !r->locked) ? EDITOR_ACTION_DISABLE
                                                   : EDITOR_ACTION_NONE;

    if (r->locked)
        return r->active_lines < r->reached_lines ? EDITOR_ACTION_ENABLE
                                                  : EDITOR_ACTION_NONE;

    if (r->reached_lines == 0)
        return EDITOR_ACTION_NONE;

    /*
     * A mixed row is safer to switch off than to complete: completing it adds
     * a load context the user may not have intended, and the confirm cannot
     * show every context. Disable first; a second press enables all lines.
     */
    if (r->status == EDITOR_PARTIAL)
        return EDITOR_ACTION_DISABLE;

    return r->active_lines == r->reached_lines ? EDITOR_ACTION_DISABLE
                                               : EDITOR_ACTION_ENABLE;
}

const char *editor_status_text(EditorStatus status)
{
    switch (status) {
    case EDITOR_LOADED:
        return "LOADED";
    case EDITOR_DISABLED:
        return "OFF";
    case EDITOR_MISSING:
        return "MISSING";
    case EDITOR_PARTIAL:
        return "MIXED";
    case EDITOR_NOT_REACHED:
        return "NOT REACHED";
    case EDITOR_BOOT:
        return "BOOT";
    }

    return "UNKNOWN";
}

const char *editor_section_text(PluginSection section)
{
    switch (section) {
    case PLUGIN_SECTION_KERNEL:
        return "KERNEL";
    case PLUGIN_SECTION_SYSTEM:
        return "SYSTEM";
    case PLUGIN_SECTION_SYSTEM_APP:
        return "SYSTEM APPS";
    case PLUGIN_SECTION_ALL_TITLES:
        return "ALL TITLES";
    case PLUGIN_SECTION_TITLE:
        return "TITLES";
    default:
        return "TITLES";
    }
}

const char *editor_row_label(int row)
{
    if (row < 0 || row >= g_editor.row_count)
        return "--";

    const EditorRow *r = &g_editor.rows[row];

    /*
     * Two mounts can hold the same basename. The row then shows the full path
     * so the confirmation names the file that will actually change.
     */
    return r->duplicate_name ? r->path : r->name;
}

const char *editor_action_text(int row)
{
    if (row < 0 || row >= g_editor.row_count)
        return "--";

    if (editor_readonly_reason() != NULL)
        return "READ ONLY";

    const EditorRow *r = &g_editor.rows[row];

    if (r->status == EDITOR_BOOT)
        return "BOOT";
    if (r->status == EDITOR_NOT_REACHED)
        return "--";
    if (r->status == EDITOR_MISSING && editor_next_action(row) < 0)
        return "MISSING";
    if (r->locked && r->active_lines == r->reached_lines)
        return "LOCKED";

    int action = editor_next_action(row);
    if (action == EDITOR_ACTION_DISABLE)
        return "DISABLE";
    if (action == EDITOR_ACTION_ENABLE)
        return "ENABLE";

    return "--";
}

const char *editor_action_reason(int row)
{
    if (row < 0 || row >= g_editor.row_count)
        return NULL;

    const char *read_only = editor_readonly_reason();
    if (read_only != NULL)
        return read_only;

    const EditorRow *r = &g_editor.rows[row];

    if (r->status == EDITOR_BOOT)
        return "THIS MODULE LOADS BEFORE TAIHEN. COCKPIT DOES NOT CHANGE IT.";
    if (r->status == EDITOR_NOT_REACHED)
        return "A HALT LINE IN THE CONFIG. THIS ROW IS NOT LOADED.";
    if (r->status == EDITOR_MISSING && editor_next_action(row) < 0)
        return "FILE IS MISSING FROM THE TAI DIRECTORY.";
    if (r->locked && r->active_lines == r->reached_lines)
        return "HENKAKU IS THE SETTINGS MENU AND RECOVERY PATH. IT CANNOT BE DISABLED.";

    if (editor_next_action(row) < 0)
        return "COCKPIT CANNOT CHANGE THIS ROW.";

    return NULL;
}

const char *editor_confirm_text(int row, int mode)
{
    if (row < 0 || row >= g_editor.row_count || (mode != 0 && mode != 1))
        return NULL;

    const EditorRow *r = &g_editor.rows[row];

    if (r->locked || r->boot || r->status == EDITOR_NOT_REACHED)
        return NULL;
    if (r->status == EDITOR_MISSING && mode == 1)
        return NULL;

    if (is_storage_driver(r->name)) {
        return mode == 0
                   ? "UX0 MAY NOT MOUNT AFTER THE REBOOT. GAMES AND APPS ON THE CARD MAY NOT START."
                   : "THE STORAGE DRIVER LOADS AGAIN.";
    }

    if (tai_ieq(r->name, "0syscall6.skprx"))
        return mode == 0 ? "HIGHER FIRMWARE GAMES MAY FAIL C1-6703-6"
                         : "PATCHES FIRMWARE AND KEY CHECKS AGAIN";
    if (tai_ieq(r->name, "nonpdrm.skprx"))
        return mode == 0 ? "NONPDRM INSTALLS FAIL LICENCE CHECKS"
                         : "NONPDRM LICENCES RETURN";
    if (tai_ieq(r->name, "repatch.skprx"))
        return mode == 0 ? "MODS IN UX0:REPATCH STOP LOADING"
                         : "MODS LOAD AGAIN";
    if (tai_ieq(r->name, "nopsmdrm.skprx"))
        return mode == 0 ? "PSM TITLES FAIL LICENCE CHECKS"
                         : "PSM LICENCES RETURN";
    if (tai_ieq(r->name, "libshacccg.suprx"))
        return mode == 0 ? "VITAGL HOMEBREW MAY FAIL TO START"
                         : "SHADER COMPILER LINE RETURNS";
    if (tai_ieq(r->name, "yamt.suprx"))
        return mode == 0 ? "YAMT STORAGE MENU DISAPPEARS"
                         : "YAMT STORAGE MENU RETURNS";
    if (tai_ieq(r->name, "yamt_helper.skprx"))
        return mode == 0 ? "YAMT REMOUNT COMMANDS FAIL"
                         : "YAMT REMOUNT COMMANDS RETURN";
    if (tai_ieq(r->name, "reVitaMotion.suprx"))
        return mode == 0 ? "REVITA GYRO REMAPPING STOPS"
                         : "REVITA GYRO REMAPPING RETURNS";

    return NULL;
}

int editor_row_for_path(const char *path)
{
    return path == NULL ? -1 : row_for_path(&g_editor, path);
}

static int copy_region(char *dst, int out, int cap, const char *src, int from,
                       int to)
{
    for (int i = from; i < to; ++i) {
        if (out >= cap - 1)
            return -1;
        dst[out++] = src[i];
    }

    return out;
}

static void rollback_activity(EditorState *e, const int *old_active)
{
    for (int i = 0; i < e->line_count; ++i)
        e->lines[i].active = old_active[i];

    rebuild_rows(e);
}

static void adopt_text(EditorState *e, const char *text, int len,
                       const int *launch, int launch_count)
{
    memcpy(e->last_text, text, (size_t)len);
    e->last_text[len] = '\0';
    e->last_len = len;
    parse_text(e, text, len, 0, launch, launch_count);
}

static EditorWriteResult write_failure(void)
{
    if (strcmp(platform_write_error(), "RESTORE") == 0)
        return EDITOR_WRITE_UNRECOVERED;

    return EDITOR_WRITE_FAILED;
}

EditorWriteResult editor_toggle(int row, int mode, char *err, int err_cap)
{
    if (err != NULL && err_cap > 0)
        err[0] = '\0';

    if (row < 0 || row >= g_editor.row_count || (mode != 0 && mode != 1)) {
        if (err != NULL)
            snprintf(err, (size_t)err_cap, "NO ACTION");
        return EDITOR_WRITE_REFUSED;
    }

    const char *read_only = editor_readonly_reason();
    if (read_only != NULL) {
        if (err != NULL)
            snprintf(err, (size_t)err_cap, "%s", read_only);
        return EDITOR_WRITE_REFUSED;
    }

    EditorRow *r = &g_editor.rows[row];

    if (r->locked && mode == 0) {
        if (err != NULL)
            snprintf(err, (size_t)err_cap, "HENKAKU IS LOCKED");
        return EDITOR_WRITE_REFUSED;
    }

    if (r->boot || r->status == EDITOR_NOT_REACHED) {
        if (err != NULL)
            snprintf(err, (size_t)err_cap, "ROW IS NOT EDITABLE");
        return EDITOR_WRITE_REFUSED;
    }

    if (r->status == EDITOR_MISSING && mode == 1) {
        /*
         * Status is cached at scan time. A file copied in over FTP after
         * launch must not be enabled against a stale MISSING verdict, and a
         * file deleted after launch must not be written active.
         */
        if (!platform_file_exists(r->path)) {
            if (err != NULL)
                snprintf(err, (size_t)err_cap, "FILE IS MISSING");
            return EDITOR_WRITE_REFUSED;
        }

        r->status = row_status(r);
    }

    int checked = 0;
    if (platform_read_config(g_editor.source, s_work, (int)sizeof(s_work),
                             &checked) != PLATFORM_READ_OK ||
        !matches_last_text(s_work, checked)) {
        if (err != NULL)
            snprintf(err, (size_t)err_cap,
                     "CONFIG CHANGED ON DISK. RESTART COCKPIT.");
        return EDITOR_WRITE_REFUSED;
    }

    int old_active[EDITOR_LINES_MAX] = {0};
    int launch_active[EDITOR_LINES_MAX] = {0};

    for (int i = 0; i < g_editor.line_count; ++i) {
        old_active[i] = g_editor.lines[i].active;
        launch_active[i] = g_editor.lines[i].launch_active;
    }

    for (int i = 0; i < g_editor.line_count; ++i) {
        EditorLine *l = &g_editor.lines[i];
        if (l->kind != EDITOR_LINE_PATH || l->row != row || !l->reached)
            continue;

        l->active = mode == 1;
    }

    int out = 0;

    for (int i = 0; i < g_editor.line_count; ++i) {
        EditorLine *l = &g_editor.lines[i];
        int line_end = l->start + l->len;

        if (l->kind == EDITOR_LINE_PATH && l->row == row && l->reached) {
            if (l->active) {
                int from = l->start;

                if (l->hash >= 0)
                    from = l->hash;

                out = copy_region(s_work, out, (int)sizeof(s_work),
                                  g_editor.text, l->start, from);
                if (out < 0)
                    break;

                if (l->hash >= 0) {
                    out = copy_region(s_work, out, (int)sizeof(s_work),
                                      g_editor.text, l->path_start, line_end);
                } else {
                    out = copy_region(s_work, out, (int)sizeof(s_work),
                                      g_editor.text, from, line_end);
                }
            } else {
                if (l->hash >= 0) {
                    out = copy_region(s_work, out, (int)sizeof(s_work),
                                      g_editor.text, l->start, line_end);
                } else {
                    out = copy_region(s_work, out, (int)sizeof(s_work),
                                      g_editor.text, l->start, l->content);
                    if (out < 0)
                        break;
                    if (out >= (int)sizeof(s_work) - 1) {
                        out = -1;
                        break;
                    }
                    s_work[out++] = '#';
                    out = copy_region(s_work, out, (int)sizeof(s_work),
                                      g_editor.text, l->content, line_end);
                }
            }
        } else {
            out = copy_region(s_work, out, (int)sizeof(s_work), g_editor.text,
                              l->start, line_end);
        }

        if (out < 0)
            break;

        out = copy_region(s_work, out, (int)sizeof(s_work), g_editor.text,
                          line_end, line_end + l->newline);
        if (out < 0)
            break;
    }

    if (out < 0 || out >= EDITOR_TEXT_MAX - 1) {
        rollback_activity(&g_editor, old_active);
        if (err != NULL)
            snprintf(err, (size_t)err_cap, "CONFIG TOO LARGE TO EDIT");
        return EDITOR_WRITE_REFUSED;
    }

    s_work[out] = '\0';

    /*
     * Keep the pre-write bytes as the one-step undo. This must exist before
     * the live file changes; if it cannot be written, the change is refused
     * rather than made without a way back (D70).
     */
    if (platform_snapshot_undo(g_editor.source, g_editor.last_text,
                               g_editor.last_len) != 0) {
        rollback_activity(&g_editor, old_active);
        if (err != NULL)
            snprintf(err, (size_t)err_cap, "WRITE FAILED UNDO");
        return EDITOR_WRITE_FAILED;
    }

    int undo_differs = g_editor.last_len != out ||
                       memcmp(g_editor.last_text, s_work, (size_t)out) != 0;

    if (platform_write_config(g_editor.source, s_work, out) != 0) {
        EditorWriteResult result = write_failure();

        rollback_activity(&g_editor, old_active);
        if (err != NULL)
            snprintf(err, (size_t)err_cap, "WRITE FAILED %s",
                     platform_write_error());
        return result;
    }

    adopt_text(&g_editor, s_work, out, launch_active, g_editor.line_count);
    g_editor.undo_available = undo_differs;

    return EDITOR_WRITE_OK;
}

EditorWriteResult editor_undo(char *err, int err_cap)
{
    int n = 0;

    if (err != NULL && err_cap > 0)
        err[0] = '\0';

    if (g_editor.source[0] == '\0') {
        if (err != NULL)
            snprintf(err, (size_t)err_cap, "NO CONFIG.TXT");
        return EDITOR_WRITE_REFUSED;
    }

    PlatformReadStatus read = platform_read_undo(
        g_editor.source, s_undo, (int)sizeof(s_undo), &n);

    if (read != PLATFORM_READ_OK) {
        if (err != NULL)
            snprintf(err, (size_t)err_cap, "%s",
                     read == PLATFORM_READ_TOO_LARGE ? "RESTORE POINT TOO LARGE"
                                                     : "NO RESTORE POINT");
        return EDITOR_WRITE_REFUSED;
    }

    /*
     * Never overwrite a file that changed since the scan. The undo is one
     * press away from a destructive write; a stale view is not permission.
     */
    if (g_editor.source_ok) {
        int live_len = 0;

        if (platform_read_config(g_editor.source, s_work,
                                 (int)sizeof(s_work),
                                 &live_len) != PLATFORM_READ_OK ||
            !matches_last_text(s_work, live_len)) {
            if (err != NULL)
                snprintf(err, (size_t)err_cap,
                         "CONFIG CHANGED ON DISK. NOTHING RESTORED.");
            return EDITOR_WRITE_REFUSED;
        }
    }

    if (platform_write_config(g_editor.source, s_undo, n) != 0) {
        EditorWriteResult result = write_failure();

        if (err != NULL)
            snprintf(err, (size_t)err_cap, "WRITE FAILED %s",
                     platform_write_error());
        return result;
    }

    /*
     * Re-read instead of patching the in-memory model when there was no live
     * config: the scan owns every derived field. When the session is intact,
     * keep the launch baseline so LOADED still describes the running set and
     * the bad session snapshot is not overwritten by the restore.
     */
    if (!g_editor.source_ok) {
        editor_scan();
        return EDITOR_WRITE_OK;
    }

    int launch_active[EDITOR_LINES_MAX] = {0};
    int line_count = g_editor.line_count;

    for (int i = 0; i < line_count; ++i)
        launch_active[i] = g_editor.lines[i].launch_active;

    adopt_text(&g_editor, s_undo, n, launch_active, line_count);
    platform_remove_undo(g_editor.source);
    g_editor.undo_available = 0;

    return EDITOR_WRITE_OK;
}
