#include "core/plugin_editor.h"

#include <stdio.h>
#include <string.h>

#include "core/platform.h"
#include "core/tai_config.h"

EditorState g_editor;

static const char *const CONFIG_UX0 = "ux0:tai/config.txt";
static const char *const CONFIG_UR0 = "ur0:tai/config.txt";
static const char *const BOOT_CONFIG = "ur0:tai/boot_config.txt";
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
    for (int i = 0; i < e->boot_count; ++i) {
        if (tai_ieq(e->boot_paths[i], path))
            return 1;
    }

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
            r->status = EDITOR_ENABLED;
        } else {
            r->status = EDITOR_DISABLED;
        }

        if (r->pending)
            ++e->pending_count;
    }

    /*
     * Display order is section-major, so the selection index must follow the
     * same order the panel draws. Without this, a *main row before a *KERNEL row
     * is drawn above it but remains a higher index, and Down moves the cursor
     * upward on screen (review #19).
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
 * several active lines must add one. Recompute it after the loop above so the
 * count does not depend on the line order.
 */
static void rebuild_counts(EditorState *e)
{
    e->activated_total = 0;
    memset(e->group_counts, 0, sizeof(e->group_counts));

    for (int i = 0; i < e->row_count; ++i) {
        EditorRow *r = &e->rows[i];
        if (r->boot)
            continue;
        if (r->status == EDITOR_ENABLED || r->status == EDITOR_PARTIAL) {
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

    int have_section = 0;
    int halted = 0;
    PluginSection section = PLUGIN_SECTION_TITLE;
    int i = 0;

    while (i < len && e->line_count < EDITOR_LINES_MAX) {
        int start = i;
        while (i < len && text[i] != '\n')
            ++i;

        int end = i;
        int newline = 0;
        if (i < len) {
            ++i;
            newline = 1;
            if (end > start && text[end - 1] == '\r') {
                --end;
                newline = 2;
            }
        }

        int content = start;
        while (content < end && (text[content] == ' ' || text[content] == '\t'))
            ++content;

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
            int active = 1;
            int path_start = content;

            if (text[content] == '#') {
                active = 0;
                path_start = content + 1;
                while (path_start < content_end &&
                       (text[path_start] == ' ' || text[path_start] == '\t'))
                    ++path_start;
            }

            if (path_start < content_end &&
                tai_is_plugin_path_slice(text + path_start,
                                     content_end - path_start)) {
                l->kind = EDITOR_LINE_PATH;
                l->active = active;
                l->reached = have_section && !halted;
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

static void parse_boot(EditorState *e, const char *text, int len)
{
    e->boot_count = 0;
    int i = 0;

    while (i < len && e->boot_count < EDITOR_BOOT_MAX) {
        while (i < len && (text[i] == ' ' || text[i] == '\t'))
            ++i;

        if (i < len && text[i] == '-')
            ++i;
        while (i < len && (text[i] == ' ' || text[i] == '\t'))
            ++i;

        if (i + 4 <= len && memcmp(text + i, "load", 4) == 0) {
            i += 4;
            while (i < len && (text[i] == ' ' || text[i] == '\t'))
                ++i;

            int start = i;
            while (i < len && text[i] != '\n' && text[i] != '\r')
                ++i;

            if (i > start) {
                tai_copy_slice(e->boot_paths[e->boot_count],
                           sizeof(e->boot_paths[0]), text + start, i - start);
                ++e->boot_count;
            }
        }

        while (i < len && text[i] != '\n')
            ++i;
        if (i < len)
            ++i;
    }
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
    if (tai_ends_with(name, ".bak") || tai_ends_with(name, ".new"))
        return;
    if (e->orphan_count >= EDITOR_ORPHANS_MAX)
        return;

    char full[PLUGIN_PATH_MAX];
    int dirlen = (int)strlen(ctx->dir);
    int namelen = (int)strlen(name);

    if (dirlen + 1 + namelen >= (int)sizeof(full))
        return;

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

void editor_scan(void)
{
    memset(&g_editor, 0, sizeof(g_editor));

    int fail_ux0 = platform_backup_rotate(CONFIG_UX0) != 0;
    int fail_ur0 = platform_backup_rotate(CONFIG_UR0) != 0;
    int fail_boot = platform_backup_rotate(BOOT_CONFIG) != 0;

    static char buf[EDITOR_TEXT_MAX];
    int n = 0;
    PlatformReadStatus status = platform_read_config(CONFIG_UX0, buf,
                                                     (int)sizeof(buf), &n);

    if (status == PLATFORM_READ_OK || status == PLATFORM_READ_TOO_LARGE) {
        tai_copy(g_editor.source, sizeof(g_editor.source), CONFIG_UX0);
        g_editor.source_ok = 1;
        if (fail_ux0)
            g_editor.read_only = 1;
    } else {
        status = platform_read_config(CONFIG_UR0, buf, (int)sizeof(buf), &n);
        if (status == PLATFORM_READ_OK || status == PLATFORM_READ_TOO_LARGE) {
            tai_copy(g_editor.source, sizeof(g_editor.source), CONFIG_UR0);
            g_editor.source_ok = 1;
            if (fail_ur0)
                g_editor.read_only = 1;
        }
    }

    if (!g_editor.source_ok) {
        g_editor.read_only = 1;
        g_editor.invalid = 1;
    } else if (status == PLATFORM_READ_TOO_LARGE) {
        g_editor.too_large = 1;
        g_editor.read_only = 1;
    } else if (status != PLATFORM_READ_OK) {
        g_editor.read_only = 1;
        g_editor.invalid = 1;
    } else if (n >= EDITOR_TEXT_MAX) {
        g_editor.too_large = 1;
        g_editor.read_only = 1;
    } else {
        tai_copy(g_editor.launch_text, sizeof(g_editor.launch_text), buf);
        g_editor.launch_len = n;
        tai_copy(g_editor.last_text, sizeof(g_editor.last_text), buf);
        g_editor.last_len = n;
        parse_text(&g_editor, buf, n, 1);
        if (g_editor.lines_truncated)
            g_editor.read_only = 1;
    }

    if (strcmp(g_editor.source, CONFIG_UX0) == 0 &&
        platform_file_exists(CONFIG_UR0))
        tai_copy(g_editor.ignored, sizeof(g_editor.ignored), CONFIG_UR0);
    else if (strcmp(g_editor.source, CONFIG_UR0) == 0 &&
             platform_file_exists(CONFIG_UX0))
        tai_copy(g_editor.ignored, sizeof(g_editor.ignored), CONFIG_UX0);

    if (fail_boot ||
        (strcmp(g_editor.source, CONFIG_UX0) == 0 && fail_ur0) ||
        (strcmp(g_editor.source, CONFIG_UR0) == 0 && fail_ux0))
        g_editor.backup_warning = 1;

    static char bootbuf[8192];
    int bn = 0;
    PlatformReadStatus boot_status = platform_read_config(
        BOOT_CONFIG, bootbuf, (int)sizeof(bootbuf), &bn);

    if (boot_status == PLATFORM_READ_OK) {
        g_editor.boot_ok = 1;
        parse_boot(&g_editor, bootbuf, bn);
    } else {
        g_editor.boot_ok = 0;
    }

    rebuild_rows(&g_editor);

    if (!g_editor.valid || !g_editor.boot_ok || g_editor.truncated) {
        g_editor.orphans_unverified = 1;
    } else {
        for (size_t i = 0; i < sizeof(TAI_DIRS) / sizeof(TAI_DIRS[0]); ++i) {
            OrphanCtx ctx = {&g_editor, TAI_DIRS[i]};
            if (platform_list_dir(TAI_DIRS[i], orphan_cb, &ctx) < 0)
                g_editor.orphans_unverified = 1;
        }
    }
}

int editor_row_actionable(int row)
{
    if (row < 0 || row >= g_editor.row_count)
        return 0;

    if (!g_editor.source_ok || g_editor.read_only || g_editor.invalid ||
        g_editor.too_large || g_editor.lines_truncated)
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

    const EditorRow *r = &g_editor.rows[row];

    if (r->status == EDITOR_BOOT || r->status == EDITOR_NOT_REACHED)
        return -1;

    if (r->locked)
        return r->active_lines < r->reached_lines ? 1 : -1;

    if (r->status == EDITOR_MISSING)
        return r->active_lines > 0 ? 0 : -1;

    int any_launch_active = 0;
    int any_launch_inactive = 0;
    int all_active = 1;
    int all_inactive = 1;
    int matches_launch = 1;
    int reached = 0;

    for (int i = 0; i < g_editor.line_count; ++i) {
        const EditorLine *l = &g_editor.lines[i];
        if (l->kind != EDITOR_LINE_PATH || l->row != row || !l->reached)
            continue;

        ++reached;
        if (l->launch_active)
            any_launch_active = 1;
        else
            any_launch_inactive = 1;

        if (l->active)
            all_inactive = 0;
        else
            all_active = 0;

        if (l->active != l->launch_active)
            matches_launch = 0;
    }

    if (reached == 0)
        return -1;

    if (any_launch_active && any_launch_inactive) {
        if (matches_launch)
            return 0;
        if (all_inactive)
            return 1;
        if (all_active)
            return 2;
        return 0;
    }

    if (all_inactive)
        return 1;
    return 0;
}

const char *editor_status_text(EditorStatus status)
{
    switch (status) {
    case EDITOR_ENABLED:
        return "ENABLED";
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

const char *editor_confirm_text(int row, int mode)
{
    if (row < 0 || row >= g_editor.row_count || mode < 0 || mode > 2)
        return NULL;

    const EditorRow *r = &g_editor.rows[row];

    if (mode == 2 || r->locked || r->boot || r->status == EDITOR_NOT_REACHED)
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


int editor_nearest_actionable(int from)
{
    if (g_editor.row_count == 0)
        return -1;

    if (from < 0)
        from = 0;

    for (int i = from; i < g_editor.row_count; ++i) {
        if (editor_row_actionable(i))
            return i;
    }

    for (int i = from - 1; i >= 0; --i) {
        if (editor_row_actionable(i))
            return i;
    }

    return -1;
}

int editor_toggle(int row, int mode, char *err, int err_cap)
{
    if (err != NULL && err_cap > 0)
        err[0] = '\0';

    if (row < 0 || row >= g_editor.row_count || mode < 0 || mode > 2) {
        if (err != NULL)
            snprintf(err, (size_t)err_cap, "NO ACTION");
        return -1;
    }

    if (!g_editor.source_ok || g_editor.read_only || g_editor.invalid ||
        g_editor.too_large || g_editor.lines_truncated) {
        if (err != NULL)
            snprintf(err, (size_t)err_cap, "CONFIG IS READ-ONLY");
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
            snprintf(err, (size_t)err_cap, "CONFIG CHANGED ON DISK. RESTART COCKPIT.");
        return -1;
    }

    char bak[PLUGIN_PATH_MAX + 8];
    snprintf(bak, sizeof(bak), "%s.bak", g_editor.source);

    static char bakbuf[EDITOR_TEXT_MAX];
    int baklen = 0;
    if (platform_read_config(bak, bakbuf, (int)sizeof(bakbuf), &baklen) !=
            PLATFORM_READ_OK ||
        baklen != g_editor.launch_len ||
        memcmp(bakbuf, g_editor.launch_text, (size_t)baklen) != 0) {
        if (err != NULL)
            snprintf(err, (size_t)err_cap, "BACKUP MISSING OR CHANGED");
        return -1;
    }

    int old[EDITOR_LINES_MAX];
    int launch[EDITOR_LINES_MAX];
    for (int i = 0; i < g_editor.line_count; ++i) {
        old[i] = g_editor.lines[i].active;
        launch[i] = g_editor.lines[i].launch_active;
    }

    for (int i = 0; i < g_editor.line_count; ++i) {
        EditorLine *l = &g_editor.lines[i];
        if (l->kind != EDITOR_LINE_PATH || l->row != row || !l->reached)
            continue;

        if (mode == 0)
            l->active = 0;
        else if (mode == 1)
            l->active = 1;
        else
            l->active = launch[i];
    }

    static char next[EDITOR_TEXT_MAX];
    int out = 0;
    int overflow = 0;

    for (int i = 0; i < g_editor.line_count && !overflow; ++i) {
        EditorLine *l = &g_editor.lines[i];
        int line_end = l->start + l->len;
        int nl = l->newline;

        if (l->kind == EDITOR_LINE_PATH && l->row == row && l->reached) {
            for (int p = l->start; p < l->content; ++p) {
                if (out >= EDITOR_TEXT_MAX - 1) {
                    overflow = 1;
                    break;
                }
                next[out++] = g_editor.text[p];
            }
            if (overflow)
                break;

            int was_active = old[i];
            int path_start = was_active ? l->content : l->content + 1;

            if (!l->active) {
                if (out >= EDITOR_TEXT_MAX - 1) {
                    overflow = 1;
                    break;
                }
                next[out++] = '#';
            }

            for (int p = path_start; p < line_end; ++p) {
                if (out >= EDITOR_TEXT_MAX - 1) {
                    overflow = 1;
                    break;
                }
                next[out++] = g_editor.text[p];
            }
        } else {
            for (int p = l->start; p < line_end; ++p) {
                if (out >= EDITOR_TEXT_MAX - 1) {
                    overflow = 1;
                    break;
                }
                next[out++] = g_editor.text[p];
            }
        }

        if (overflow)
            break;

        for (int p = 0; p < nl; ++p) {
            if (out >= EDITOR_TEXT_MAX - 1) {
                overflow = 1;
                break;
            }
            next[out++] = g_editor.text[line_end + p];
        }
    }

    if (overflow || out >= EDITOR_TEXT_MAX - 1) {
        for (int i = 0; i < g_editor.line_count; ++i)
            g_editor.lines[i].active = old[i];
        rebuild_rows(&g_editor);
        if (err != NULL)
            snprintf(err, (size_t)err_cap, "CONFIG TOO LARGE TO EDIT");
        return -1;
    }

    if (platform_write_config(g_editor.source, next, out) != 0) {
        for (int i = 0; i < g_editor.line_count; ++i)
            g_editor.lines[i].active = old[i];
        rebuild_rows(&g_editor);
        if (err != NULL)
            snprintf(err, (size_t)err_cap, "WRITE FAILED");
        return -1;
    }

    static char launch_text[EDITOR_TEXT_MAX];
    int launch_len = g_editor.launch_len;
    memcpy(launch_text, g_editor.launch_text, (size_t)launch_len + 1);

    int line_count = g_editor.line_count;
    parse_text(&g_editor, next, out, 0);

    for (int i = 0; i < line_count && i < g_editor.line_count; ++i)
        g_editor.lines[i].launch_active = launch[i];

    memcpy(g_editor.launch_text, launch_text, (size_t)launch_len + 1);
    g_editor.launch_len = launch_len;
    tai_copy(g_editor.last_text, sizeof(g_editor.last_text), next);
    g_editor.last_len = out;

    rebuild_rows(&g_editor);

    return 0;
}
