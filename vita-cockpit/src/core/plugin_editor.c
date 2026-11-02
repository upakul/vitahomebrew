#include "core/plugin_editor.h"

#include <stdio.h>
#include <string.h>

#include "core/platform.h"
#include "core/tai_config.h"

EditorState g_editor;

static const char *const CONFIG_UX0 = "ux0:tai/config.txt";
static const char *const CONFIG_UR0 = "ur0:tai/config.txt";
static const char *const TAI_DIRS[2] = {"ur0:tai", "ux0:tai"};

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

static void rebuild_rows(EditorState *e)
{
    memset(e->rows, 0, sizeof(e->rows));
    e->row_count = 0;
    e->pending_count = 0;
    e->activated_total = 0;
    memset(e->group_counts, 0, sizeof(e->group_counts));

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
        }

        if (l->active != l->launch_active)
            r->pending = 1;

        l->row = ri;
    }

    for (int i = 0; i < e->row_count; ++i) {
        EditorRow *r = &e->rows[i];

        r->locked = tai_ieq(r->name, "henkaku.suprx");
        r->boot = is_boot_path(e, r->path);

        if (r->boot) {
            r->status = EDITOR_BOOT;
        } else if (r->reached_lines == 0) {
            r->status = EDITOR_NOT_REACHED;
        } else if (!platform_file_exists(r->path)) {
            r->status = EDITOR_MISSING;
        } else if (r->active_lines > 0 && r->active_lines < r->reached_lines) {
            r->status = EDITOR_PARTIAL;
        } else if (r->active_lines > 0) {
            r->status = EDITOR_ACTIVATED;
        } else {
            r->status = EDITOR_DISABLED;
        }

        if (r->pending)
            ++e->pending_count;
    }

    /*
     * Display order is section-major, so the selection index must follow the
     * same order the panel draws. The sort is stable: rows in one section keep
     * the order their first line appeared in the file.
     */
    for (int i = 1; i < e->row_count; ++i) {
        EditorRow tmp = e->rows[i];
        int j = i - 1;

        while (j >= 0 && e->rows[j].section > tmp.section) {
            e->rows[j + 1] = e->rows[j];
            --j;
        }
        e->rows[j + 1] = tmp;
    }

    for (int i = 0; i < e->line_count; ++i) {
        EditorLine *l = &e->lines[i];
        if (l->kind == EDITOR_LINE_PATH)
            l->row = row_for_path(e, l->path);
    }

    rebuild_counts(e);
}

/*
 * The activated total is counted once per row, not once per line. A row with
 * several active lines must add one.
 */
static void rebuild_counts(EditorState *e)
{
    e->activated_total = 0;
    memset(e->group_counts, 0, sizeof(e->group_counts));

    for (int i = 0; i < e->row_count; ++i) {
        EditorRow *r = &e->rows[i];
        if (r->boot)
            continue;
        if (r->status == EDITOR_ACTIVATED || r->status == EDITOR_PARTIAL) {
            ++e->activated_total;
            ++e->group_counts[r->section];
        }
    }
}

static void parse_text(EditorState *e, const char *text, int len, int initial)
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
                l->kind = EDITOR_LINE_PATH;
                l->active = active;
                l->reached = have_section && !halted;
                l->hash = hash;
                l->hash_count = hash_count;
                l->path_start = path_start;

                if (content_end - path_start >= PLUGIN_PATH_MAX)
                    e->lines_truncated = 1;

                tai_copy_slice(l->path, sizeof(l->path), text + path_start,
                               content_end - path_start);
                if (!have_section)
                    e->valid = 0;
            } else {
                l->kind = EDITOR_LINE_COMMENT;
            }
        }

        if (initial)
            l->launch_active = l->active;

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
        tai_ends_with(name, ".session"))
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

void editor_scan(void)
{
    memset(&g_editor, 0, sizeof(g_editor));

    static char buf[EDITOR_TEXT_MAX];
    int n = 0;
    int saw_unreadable = 0;

    PlatformReadStatus status = platform_read_config(CONFIG_UX0, buf,
                                                     (int)sizeof(buf), &n);

    if (status == PLATFORM_READ_UNREADABLE)
        saw_unreadable = 1;

    if (status == PLATFORM_READ_OK || status == PLATFORM_READ_TOO_LARGE) {
        tai_copy(g_editor.source, sizeof(g_editor.source), CONFIG_UX0);
        g_editor.source_ok = 1;
    } else {
        status = platform_read_config(CONFIG_UR0, buf, (int)sizeof(buf), &n);
        if (status == PLATFORM_READ_UNREADABLE)
            saw_unreadable = 1;
        if (status == PLATFORM_READ_OK || status == PLATFORM_READ_TOO_LARGE) {
            tai_copy(g_editor.source, sizeof(g_editor.source), CONFIG_UR0);
            g_editor.source_ok = 1;
        }
    }

    if (!g_editor.source_ok) {
        g_editor.read_only = 1;
        g_editor.invalid = 1;
        g_editor.unreadable = saw_unreadable;
    } else if (status == PLATFORM_READ_TOO_LARGE) {
        g_editor.too_large = 1;
        g_editor.read_only = 1;
        g_editor.backup_warning = 1;
    } else if (status != PLATFORM_READ_OK) {
        g_editor.read_only = 1;
        g_editor.unreadable = 1;
        g_editor.invalid = 1;
        g_editor.backup_warning = 1;
    } else if (n >= EDITOR_TEXT_MAX) {
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

        if (platform_backup_config(g_editor.source, buf, n, &original_ok,
                                   &session_ok) != 0 ||
            !original_ok || !session_ok) {
            g_editor.backup_warning = 1;
        }

        g_editor.backup_original_ok = original_ok;
        g_editor.backup_session_ok = session_ok;

        memcpy(g_editor.last_text, buf, (size_t)n);
        g_editor.last_text[n] = '\0';
        g_editor.last_len = n;
        parse_text(&g_editor, buf, n, 1);

        if (g_editor.lines_truncated || g_editor.rows_truncated)
            g_editor.read_only = 1;
    }

    if (strcmp(g_editor.source, CONFIG_UX0) == 0 &&
        platform_file_exists(CONFIG_UR0))
        tai_copy(g_editor.ignored, sizeof(g_editor.ignored), CONFIG_UR0);
    else if (strcmp(g_editor.source, CONFIG_UR0) == 0 &&
             platform_file_exists(CONFIG_UX0))
        tai_copy(g_editor.ignored, sizeof(g_editor.ignored), CONFIG_UX0);

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
        return "CONFIG TOO LONG";
    if (g_editor.rows_truncated)
        return "TOO MANY PLUGIN ROWS";
    if (g_editor.read_only)
        return "CONFIG IS READ ONLY";

    return NULL;
}

int editor_row_actionable(int row)
{
    if (row < 0 || row >= g_editor.row_count)
        return 0;

    if (!g_editor.source_ok || g_editor.read_only || g_editor.too_large ||
        g_editor.lines_truncated || g_editor.rows_truncated)
        return 0;

    const EditorRow *r = &g_editor.rows[row];

    if (r->status == EDITOR_BOOT || r->status == EDITOR_NOT_REACHED)
        return 0;
    if (r->status == EDITOR_MISSING && r->active_lines == 0)
        return 0;
    if (r->locked && r->active_lines == r->reached_lines)
        return 0;

    return editor_next_action(row) >= 0;
}

int editor_next_action(int row)
{
    if (row < 0 || row >= g_editor.row_count)
        return -1;

    if (!g_editor.source_ok || g_editor.read_only || g_editor.too_large ||
        g_editor.lines_truncated || g_editor.rows_truncated)
        return -1;

    const EditorRow *r = &g_editor.rows[row];

    if (r->status == EDITOR_BOOT || r->status == EDITOR_NOT_REACHED)
        return -1;

    if (r->locked)
        return r->active_lines < r->reached_lines ? 1 : -1;

    if (r->status == EDITOR_MISSING)
        return r->active_lines > 0 ? 0 : -1;

    if (r->reached_lines == 0)
        return -1;

    return r->active_lines == r->reached_lines ? 0 : 1;
}

const char *editor_status_text(EditorStatus status)
{
    switch (status) {
    case EDITOR_ACTIVATED:
        return "ACTIVATED";
    case EDITOR_DISABLED:
        return "DISABLED";
    case EDITOR_MISSING:
        return "MISSING";
    case EDITOR_PARTIAL:
        return "PARTIAL";
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

const char *editor_action_text(int row)
{
    if (row < 0 || row >= g_editor.row_count)
        return "--";

    if (editor_readonly_reason() != NULL)
        return "LOCKED";

    const EditorRow *r = &g_editor.rows[row];

    if (r->status == EDITOR_BOOT)
        return "BOOT";
    if (r->status == EDITOR_NOT_REACHED)
        return "--";
    if (r->locked && r->active_lines == r->reached_lines)
        return "LOCKED";
    if (r->status == EDITOR_MISSING && r->active_lines == 0)
        return "MISSING";

    int action = editor_next_action(row);
    if (action == 0)
        return "DISABLE";
    if (action == 1)
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
        return "BOOT MODULE. NOT EDITABLE HERE.";
    if (r->status == EDITOR_NOT_REACHED)
        return "AFTER A *! HALT. NOT LOADED.";
    if (r->locked && r->active_lines == r->reached_lines)
        return "HENKAKU IS THE SETTINGS UI. IT CANNOT BE DISABLED.";
    if (r->status == EDITOR_MISSING && r->active_lines == 0)
        return "FILE IS MISSING FROM THE TAI DIRECTORY.";

    if (editor_next_action(row) < 0)
        return "THIS ROW HAS NO ACTION.";

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
                   ? "UX0 MAY NOT MOUNT NEXT BOOT. RECOVER WITH CARD OR PC."
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

int editor_toggle(int row, int mode, char *err, int err_cap)
{
    if (err != NULL && err_cap > 0)
        err[0] = '\0';

    if (row < 0 || row >= g_editor.row_count || (mode != 0 && mode != 1)) {
        if (err != NULL)
            snprintf(err, (size_t)err_cap, "NO ACTION");
        return -1;
    }

    const char *read_only = editor_readonly_reason();
    if (read_only != NULL) {
        if (err != NULL)
            snprintf(err, (size_t)err_cap, "%s", read_only);
        return -1;
    }

    EditorRow *r = &g_editor.rows[row];

    if (r->locked && mode == 0) {
        if (err != NULL)
            snprintf(err, (size_t)err_cap, "HENKAKU IS LOCKED");
        return -1;
    }

    if (r->boot || r->status == EDITOR_NOT_REACHED) {
        if (err != NULL)
            snprintf(err, (size_t)err_cap, "ROW IS NOT EDITABLE");
        return -1;
    }

    if (r->status == EDITOR_MISSING && mode == 1) {
        if (err != NULL)
            snprintf(err, (size_t)err_cap, "FILE IS MISSING");
        return -1;
    }

    static char check[EDITOR_TEXT_MAX];
    int checked = 0;
    if (platform_read_config(g_editor.source, check, (int)sizeof(check),
                             &checked) != PLATFORM_READ_OK ||
        checked != g_editor.last_len ||
        memcmp(check, g_editor.last_text, (size_t)checked) != 0) {
        if (err != NULL)
            snprintf(err, (size_t)err_cap,
                     "CONFIG CHANGED ON DISK. RESTART COCKPIT.");
        return -1;
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

    static char next[EDITOR_TEXT_MAX];
    int out = 0;

    for (int i = 0; i < g_editor.line_count; ++i) {
        EditorLine *l = &g_editor.lines[i];
        int line_end = l->start + l->len;

        if (l->kind == EDITOR_LINE_PATH && l->row == row && l->reached) {
            if (l->active) {
                int from = l->start;

                if (l->hash >= 0)
                    from = l->hash;

                out = copy_region(next, out, (int)sizeof(next), g_editor.text,
                                  l->start, from);
                if (out < 0)
                    break;

                if (l->hash >= 0) {
                    out = copy_region(next, out, (int)sizeof(next),
                                      g_editor.text, l->path_start, line_end);
                    if (out < 0)
                        break;
                } else {
                    out = copy_region(next, out, (int)sizeof(next),
                                      g_editor.text, from, line_end);
                    if (out < 0)
                        break;
                }
            } else {
                if (l->hash >= 0) {
                    out = copy_region(next, out, (int)sizeof(next),
                                      g_editor.text, l->start, line_end);
                } else {
                    out = copy_region(next, out, (int)sizeof(next),
                                      g_editor.text, l->start, l->content);
                    if (out < 0)
                        break;
                    if (out >= (int)sizeof(next) - 1) {
                        out = -1;
                        break;
                    }
                    next[out++] = '#';
                    out = copy_region(next, out, (int)sizeof(next),
                                      g_editor.text, l->content, line_end);
                }
                if (out < 0)
                    break;
            }
        } else {
            out = copy_region(next, out, (int)sizeof(next), g_editor.text,
                              l->start, line_end);
            if (out < 0)
                break;
        }

        out = copy_region(next, out, (int)sizeof(next), g_editor.text,
                          line_end, line_end + l->newline);
        if (out < 0)
            break;
    }

    if (out < 0 || out >= EDITOR_TEXT_MAX - 1) {
        for (int i = 0; i < g_editor.line_count; ++i)
            g_editor.lines[i].active = old_active[i];
        rebuild_rows(&g_editor);
        if (err != NULL)
            snprintf(err, (size_t)err_cap, "CONFIG TOO LARGE TO EDIT");
        return -1;
    }

    next[out] = '\0';

    if (platform_write_config(g_editor.source, next, out) != 0) {
        for (int i = 0; i < g_editor.line_count; ++i)
            g_editor.lines[i].active = old_active[i];
        rebuild_rows(&g_editor);
        if (err != NULL)
            snprintf(err, (size_t)err_cap, "WRITE FAILED");
        return -1;
    }

    int line_count = g_editor.line_count;
    parse_text(&g_editor, next, out, 0);

    for (int i = 0; i < line_count && i < g_editor.line_count; ++i)
        g_editor.lines[i].launch_active = launch_active[i];

    memcpy(g_editor.last_text, next, (size_t)out);
    g_editor.last_text[out] = '\0';
    g_editor.last_len = out;

    /* parse_text rebuilt rows with launch_active at zero; rebuild after the
     * session baseline is restored so PENDING means what it says. */
    rebuild_rows(&g_editor);

    return 0;
}
