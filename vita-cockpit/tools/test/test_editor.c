#include <stdio.h>
#include <string.h>

#include "core/input.h"
#include "core/plugin_editor.h"
#include "core/platform.h"
#include "core/tai_config.h"
#include "fake_platform.h"

static int g_checks;
static int g_failures;

#define CHECK(cond)                                                      \
    do {                                                                 \
        ++g_checks;                                                      \
        if (!(cond)) {                                                   \
            ++g_failures;                                                \
            fprintf(stderr, "FAIL %s:%d  %s\n", __FILE__, __LINE__,      \
                    #cond);                                              \
        }                                                                \
    } while (0)

#define SAMPLE                                                              \
    "# sample tai config\n"                                                 \
    "*KERNEL\n"                                                             \
    "ur0:tai/kubridge.skprx\n"                                              \
    "ur0:tai/storagemgr.skprx\n"                                           \
    "ur0:tai/0syscall6.skprx\n"                                            \
    "*main\n"                                                               \
    "ur0:tai/henkaku.suprx\n"                                              \
    "ur0:tai/libshacccg.suprx\n"                                           \
    "#ur0:tai/shellbat.suprx\n"                                            \
    "#ur0:tai/mystery.suprx\n"                                             \
    "*NPXS10015\n"                                                          \
    "ur0:tai/yamt.suprx\n"                                                 \
    "*ALL\n"                                                                \
    "ur0:tai/vita3g-disable.suprx\n"

#define SAMPLE_BOOT "- load\tur0:tai/taihen.skprx\n- load\tur0:tai/henkaku.skprx\n"

static int text_is(const char *path, const char *expected)
{
    static char buf[32768];
    int n = 0;

    if (platform_read_config(path, buf, (int)sizeof(buf), &n) !=
        PLATFORM_READ_OK)
        return 0;

    return strcmp(buf, expected) == 0;
}

static int read_file(const char *path, char *buf, int cap)
{
    int n = 0;

    if (platform_read_config(path, buf, cap, &n) != PLATFORM_READ_OK)
        return 0;

    return n;
}

static void test_scan_sample(void)
{
    fake_reset();
    editor_scan();

    CHECK(g_editor.source_ok == 1);
    CHECK(strcmp(g_editor.source, "ur0:tai/config.txt") == 0);
    CHECK(g_editor.valid == 1);
    CHECK(g_editor.row_count == 9);
    CHECK(g_editor.orphan_count == 1);
    CHECK(strcmp(g_editor.orphans[0], "ur0:tai/extra.skprx") == 0);
    CHECK(g_editor.activated_total == 7);
    CHECK(g_editor.group_counts[PLUGIN_SECTION_KERNEL] == 3);
    CHECK(g_editor.group_counts[PLUGIN_SECTION_SYSTEM] == 2);
    CHECK(g_editor.group_counts[PLUGIN_SECTION_SYSTEM_APP] == 1);
    CHECK(g_editor.group_counts[PLUGIN_SECTION_ALL_TITLES] == 1);

    CHECK(text_is("ur0:tai/config.txt.original", SAMPLE));
    CHECK(text_is("ur0:tai/config.txt.session", SAMPLE));
    CHECK(text_is("ur0:tai/boot_config.txt", SAMPLE_BOOT));
    CHECK(!platform_file_exists("ur0:tai/boot_config.txt.bak"));
    CHECK(!platform_file_exists("ur0:tai/boot_config.txt.original"));
    CHECK(!platform_file_exists("ur0:tai/boot_config.txt.session"));

    int henkaku = editor_row_for_path("ur0:tai/henkaku.suprx");
    CHECK(henkaku >= 0);
    CHECK(g_editor.rows[henkaku].locked == 1);
    CHECK(editor_next_action(henkaku) == -1);
    CHECK(strcmp(editor_action_text(henkaku), "LOCKED") == 0);

    int storage = editor_row_for_path("ur0:tai/storagemgr.skprx");
    CHECK(storage >= 0);
    CHECK(editor_confirm_text(storage, 0) != NULL);
    CHECK(editor_confirm_text(storage, 1) != NULL);

    CHECK(editor_confirm_text(editor_row_for_path("ur0:tai/kubridge.skprx"),
                              1) == NULL);

    int shellbat = editor_row_for_path("ur0:tai/shellbat.suprx");
    CHECK(shellbat >= 0);
    CHECK(g_editor.rows[shellbat].status == EDITOR_DISABLED);
    CHECK(editor_next_action(shellbat) == 1);
    CHECK(strcmp(editor_action_text(shellbat), "ENABLE") == 0);
    CHECK(editor_row_actionable(shellbat) == 1);

    int yamt = editor_row_for_path("ur0:tai/yamt.suprx");
    CHECK(yamt >= 0);
    CHECK(g_editor.rows[yamt].status == EDITOR_ACTIVATED);
    CHECK(editor_next_action(yamt) == 0);
}

static void test_toggle_round_trip(void)
{
    fake_reset();
    editor_scan();

    char before[32768];
    CHECK(read_file("ur0:tai/config.txt", before, (int)sizeof(before)) > 0);

    int row = editor_row_for_path("ur0:tai/kubridge.skprx");
    CHECK(row == 0);
    CHECK(editor_next_action(row) == 0);
    CHECK(editor_confirm_text(row, 0) == NULL);

    char err[96];
    CHECK(editor_toggle(row, 0, err, (int)sizeof(err)) == 0);
    CHECK(g_editor.rows[row].status == EDITOR_DISABLED);
    CHECK(g_editor.pending_count == 1);
    CHECK(g_editor.rows[row].pending == 1);

    CHECK(text_is("ur0:tai/config.txt.original", SAMPLE));
    CHECK(text_is("ur0:tai/config.txt.session", SAMPLE));
    CHECK(text_is("ur0:tai/config.txt.bak", before) == 0);

    CHECK(text_is("ur0:tai/config.txt",
                  "# sample tai config\n"
                  "*KERNEL\n"
                  "#ur0:tai/kubridge.skprx\n"
                  "ur0:tai/storagemgr.skprx\n"
                  "ur0:tai/0syscall6.skprx\n"
                  "*main\n"
                  "ur0:tai/henkaku.suprx\n"
                  "ur0:tai/libshacccg.suprx\n"
                  "#ur0:tai/shellbat.suprx\n"
                  "#ur0:tai/mystery.suprx\n"
                  "*NPXS10015\n"
                  "ur0:tai/yamt.suprx\n"
                  "*ALL\n"
                  "ur0:tai/vita3g-disable.suprx\n"));

    CHECK(editor_toggle(row, 1, err, (int)sizeof(err)) == 0);
    CHECK(g_editor.rows[row].status == EDITOR_ACTIVATED);
    CHECK(g_editor.pending_count == 0);
    CHECK(text_is("ur0:tai/config.txt", before));
}

static void test_hash_exactness(void)
{
    char err[96];

    /* Indentation stays; the '#' goes immediately before the path. */
    fake_reset();
    fake_add_file("ur0:tai/config.txt",
                  "*main\n  ur0:tai/kubridge.skprx\t\n");
    editor_scan();
    int row = editor_row_for_path("ur0:tai/kubridge.skprx");
    CHECK(row >= 0);
    CHECK(editor_toggle(row, 0, err, (int)sizeof(err)) == 0);
    CHECK(text_is("ur0:tai/config.txt", "*main\n  #ur0:tai/kubridge.skprx\t\n"));
    CHECK(editor_toggle(row, 1, err, (int)sizeof(err)) == 0);
    CHECK(text_is("ur0:tai/config.txt", "*main\n  ur0:tai/kubridge.skprx\t\n"));

    /* CRLF and a missing final newline survive. */
    fake_reset();
    fake_add_file("ur0:tai/config.txt",
                  "*main\r\nur0:tai/kubridge.skprx\r\n");
    editor_scan();
    row = editor_row_for_path("ur0:tai/kubridge.skprx");
    CHECK(editor_toggle(row, 0, err, (int)sizeof(err)) == 0);
    CHECK(text_is("ur0:tai/config.txt",
                  "*main\r\n#ur0:tai/kubridge.skprx\r\n"));

    fake_reset();
    fake_add_file("ur0:tai/config.txt", "*main\nur0:tai/kubridge.skprx");
    editor_scan();
    row = editor_row_for_path("ur0:tai/kubridge.skprx");
    CHECK(editor_toggle(row, 0, err, (int)sizeof(err)) == 0);
    CHECK(text_is("ur0:tai/config.txt", "*main\n#ur0:tai/kubridge.skprx"));

    /* A double-commented line enables by removing the whole run. */
    fake_reset();
    fake_add_file("ur0:tai/config.txt", "*main\n##ur0:tai/kubridge.skprx\n");
    editor_scan();
    row = editor_row_for_path("ur0:tai/kubridge.skprx");
    CHECK(row >= 0);
    CHECK(g_editor.rows[row].status == EDITOR_DISABLED);
    CHECK(editor_toggle(row, 1, err, (int)sizeof(err)) == 0);
    CHECK(text_is("ur0:tai/config.txt", "*main\nur0:tai/kubridge.skprx\n"));

    fake_reset();
    fake_add_file("ur0:tai/config.txt", "*main\n# # ur0:tai/kubridge.skprx\n");
    editor_scan();
    row = editor_row_for_path("ur0:tai/kubridge.skprx");
    CHECK(editor_toggle(row, 1, err, (int)sizeof(err)) == 0);
    CHECK(text_is("ur0:tai/config.txt", "*main\nur0:tai/kubridge.skprx\n"));

    /* One path in two sections is one row and both lines move together. */
    fake_reset();
    fake_add_file("ur0:tai/config.txt",
                  "*main\nur0:tai/kubridge.skprx\n"
                  "*NPXS10015\nur0:tai/kubridge.skprx\n");
    editor_scan();
    row = editor_row_for_path("ur0:tai/kubridge.skprx");
    CHECK(g_editor.row_count == 1);
    CHECK(g_editor.rows[row].line_count == 2);
    CHECK(editor_toggle(row, 0, err, (int)sizeof(err)) == 0);
    CHECK(text_is("ur0:tai/config.txt",
                  "*main\n#ur0:tai/kubridge.skprx\n"
                  "*NPXS10015\n#ur0:tai/kubridge.skprx\n"));
}

static void test_backups(void)
{
    fake_reset();
    editor_scan();

    CHECK(text_is("ur0:tai/config.txt.original", SAMPLE));
    CHECK(text_is("ur0:tai/config.txt.session", SAMPLE));

    int row = editor_row_for_path("ur0:tai/kubridge.skprx");
    char err[96];
    CHECK(editor_toggle(row, 0, err, (int)sizeof(err)) == 0);

    /* A second session refreshes the session snapshot, never the original. */
    editor_scan();
    CHECK(text_is("ur0:tai/config.txt.original", SAMPLE));
    CHECK(!text_is("ur0:tai/config.txt.session", SAMPLE));
    CHECK(text_is("ur0:tai/config.txt.session",
                  "# sample tai config\n"
                  "*KERNEL\n"
                  "#ur0:tai/kubridge.skprx\n"
                  "ur0:tai/storagemgr.skprx\n"
                  "ur0:tai/0syscall6.skprx\n"
                  "*main\n"
                  "ur0:tai/henkaku.suprx\n"
                  "ur0:tai/libshacccg.suprx\n"
                  "#ur0:tai/shellbat.suprx\n"
                  "#ur0:tai/mystery.suprx\n"
                  "*NPXS10015\n"
                  "ur0:tai/yamt.suprx\n"
                  "*ALL\n"
                  "ur0:tai/vita3g-disable.suprx\n"));
    CHECK(!platform_file_exists("ur0:tai/config.txt.bak"));
    CHECK(!platform_file_exists("ur0:tai/config.txt.1.bak"));
    CHECK(!platform_file_exists("ur0:tai/config.txt.2.bak"));

    /* A failed snapshot warns but does not lock the editor. */
    fake_reset();
    fake_fail_backup(1);
    editor_scan();
    CHECK(g_editor.backup_warning == 1);
    CHECK(g_editor.read_only == 0);
    CHECK(g_editor.backup_original_ok == 0);
    CHECK(g_editor.backup_session_ok == 0);
    row = editor_row_for_path("ur0:tai/kubridge.skprx");
    CHECK(editor_row_actionable(row) == 1);
    CHECK(editor_toggle(row, 0, err, (int)sizeof(err)) == 0);
    fake_fail_backup(0);
}

static void test_write_failures(void)
{
    char err[96];

    fake_reset();
    editor_scan();
    char before[32768];
    CHECK(read_file("ur0:tai/config.txt", before, (int)sizeof(before)) > 0);
    int row = editor_row_for_path("ur0:tai/kubridge.skprx");

    fake_fail_write(1);
    CHECK(editor_toggle(row, 0, err, (int)sizeof(err)) != 0);
    CHECK(strstr(err, "DATA") != NULL);
    CHECK(text_is("ur0:tai/config.txt", before));
    CHECK(!platform_file_exists("ur0:tai/config.txt.new"));
    CHECK(!platform_file_exists("ur0:tai/config.txt.old"));
    fake_fail_write(0);

    fake_fail_rename(1);
    CHECK(editor_toggle(row, 0, err, (int)sizeof(err)) != 0);
    CHECK(strstr(err, "WRITE FAILED") != NULL);
    CHECK(strstr(err, "RENAME") != NULL);
    CHECK(text_is("ur0:tai/config.txt", before));
    CHECK(!platform_file_exists("ur0:tai/config.txt.new"));
    CHECK(!platform_file_exists("ur0:tai/config.txt.old"));
    fake_fail_rename(0);

    CHECK(editor_toggle(row, 0, err, (int)sizeof(err)) == 0);
}

static void test_safety_refusals(void)
{
    fake_reset();
    editor_scan();
    int row = editor_row_for_path("ur0:tai/kubridge.skprx");
    char err[96];

    fake_add_file("ur0:tai/config.txt", "# changed\n");
    CHECK(editor_toggle(row, 0, err, (int)sizeof(err)) != 0);
    CHECK(strstr(err, "CONFIG CHANGED") != NULL);

    /* A too-long config is read-only with a named reason. */
    char cfg[EDITOR_LINES_MAX * 24 + 64];
    size_t off = 0;
    off += (size_t)snprintf(cfg + off, sizeof(cfg) - off, "*KERNEL\n");
    for (int i = 0; i < EDITOR_LINES_MAX + 1; ++i)
        off += (size_t)snprintf(cfg + off, sizeof(cfg) - off,
                                "ur0:tai/p%03d.skprx\n", i);
    fake_reset();
    fake_add_file("ur0:tai/config.txt", cfg);
    editor_scan();
    CHECK(g_editor.lines_truncated == 1);
    CHECK(g_editor.read_only == 1);
    CHECK(strcmp(editor_readonly_reason(), "CONFIG TOO LONG") == 0);
    CHECK(editor_row_actionable(0) == 0);
    CHECK(editor_toggle(0, 0, err, (int)sizeof(err)) != 0);

    /* No config at all. */
    fake_reset();
    fake_remove_file("ur0:tai/config.txt");
    editor_scan();
    CHECK(g_editor.source_ok == 0);
    CHECK(strcmp(editor_readonly_reason(), "NO CONFIG.TXT") == 0);

    /* An unreadable config. */
    fake_reset();
    fake_fail_read(1);
    editor_scan();
    CHECK(g_editor.unreadable == 1);
    CHECK(strcmp(editor_readonly_reason(), "CONFIG UNREADABLE") == 0);
    fake_fail_read(0);
}

static void test_statuses(void)
{
    char err[96];

    /* Partial: one active line, one commented. ENABLE completes the row. */
    fake_reset();
    fake_add_file("ur0:tai/config.txt",
                  "*main\nur0:tai/kubridge.skprx\n"
                  "*NPXS10015\n#ur0:tai/kubridge.skprx\n");
    editor_scan();
    int row = editor_row_for_path("ur0:tai/kubridge.skprx");
    CHECK(g_editor.rows[row].status == EDITOR_PARTIAL);
    CHECK(editor_next_action(row) == 1);
    CHECK(strcmp(editor_action_text(row), "ENABLE") == 0);
    CHECK(editor_toggle(row, 1, err, (int)sizeof(err)) == 0);
    CHECK(g_editor.rows[row].status == EDITOR_ACTIVATED);
    CHECK(editor_toggle(row, 0, err, (int)sizeof(err)) == 0);
    CHECK(g_editor.rows[row].status == EDITOR_DISABLED);

    /* A missing file that is still active can be disabled. */
    fake_reset();
    fake_add_file("ur0:tai/config.txt", "*KERNEL\nur0:tai/absent.skprx\n");
    editor_scan();
    row = editor_row_for_path("ur0:tai/absent.skprx");
    CHECK(row >= 0);
    CHECK(g_editor.rows[row].status == EDITOR_MISSING);
    CHECK(editor_next_action(row) == 0);
    CHECK(strcmp(editor_action_text(row), "DISABLE") == 0);
    CHECK(editor_row_actionable(row) == 1);
    CHECK(editor_toggle(row, 0, err, (int)sizeof(err)) == 0);
    CHECK(g_editor.rows[row].status == EDITOR_MISSING);
    CHECK(editor_next_action(row) == -1);
    CHECK(strcmp(editor_action_text(row), "MISSING") == 0);
    CHECK(editor_row_actionable(row) == 0);
    CHECK(editor_toggle(row, 1, err, (int)sizeof(err)) != 0);
    CHECK(strstr(err, "FILE IS MISSING") != NULL);

    /* A missing file that is commented cannot be enabled. */
    fake_reset();
    fake_add_file("ur0:tai/config.txt", "*KERNEL\n#ur0:tai/absent.skprx\n");
    editor_scan();
    row = editor_row_for_path("ur0:tai/absent.skprx");
    CHECK(row >= 0);
    CHECK(g_editor.rows[row].status == EDITOR_MISSING);
    CHECK(editor_next_action(row) == -1);
    CHECK(editor_row_actionable(row) == 0);
    CHECK(strcmp(editor_action_text(row), "MISSING") == 0);
    CHECK(strstr(editor_action_reason(row), "MISSING") != NULL);

    /* A boot module is locked by name, without reading boot_config. */
    fake_reset();
    fake_add_file("ur0:tai/config.txt", "*KERNEL\nur0:tai/taihen.skprx\n");
    editor_scan();
    row = editor_row_for_path("ur0:tai/taihen.skprx");
    CHECK(g_editor.rows[row].status == EDITOR_BOOT);
    CHECK(editor_next_action(row) == -1);
    CHECK(strcmp(editor_action_text(row), "BOOT") == 0);

    /* A line after *! is not reached and is not editable. */
    fake_reset();
    fake_add_file("ur0:tai/config.txt",
                  "*KERNEL\nur0:tai/kubridge.skprx\n*!ALL\n"
                  "ur0:tai/extra.skprx\n");
    editor_scan();
    row = editor_row_for_path("ur0:tai/extra.skprx");
    CHECK(g_editor.rows[row].status == EDITOR_NOT_REACHED);
    CHECK(editor_next_action(row) == -1);
    CHECK(strcmp(editor_action_text(row), "--") == 0);

    /* Henkaku can be restored but not disabled. */
    fake_reset();
    fake_add_file("ur0:tai/config.txt", "*main\n#ur0:tai/henkaku.suprx\n");
    editor_scan();
    row = editor_row_for_path("ur0:tai/henkaku.suprx");
    CHECK(g_editor.rows[row].locked == 1);
    CHECK(editor_next_action(row) == 1);
    CHECK(editor_toggle(row, 1, err, (int)sizeof(err)) == 0);
    CHECK(editor_next_action(row) == -1);
    CHECK(editor_toggle(row, 0, err, (int)sizeof(err)) != 0);
}

static void test_bom_and_invalid(void)
{
    char err[96];

    /* A BOM must not hide the first section or make the file read-only. */
    fake_reset();
    fake_add_file("ur0:tai/config.txt",
                  "\xEF\xBB\xBF*KERNEL\nur0:tai/kubridge.skprx\n");
    editor_scan();
    CHECK(g_editor.valid == 1);
    CHECK(g_editor.invalid == 0);
    CHECK(g_editor.read_only == 0);
    CHECK(g_editor.row_count == 1);
    int row = editor_row_for_path("ur0:tai/kubridge.skprx");
    CHECK(row >= 0);
    CHECK(editor_toggle(row, 0, err, (int)sizeof(err)) == 0);

    char after[32768];
    CHECK(read_file("ur0:tai/config.txt", after, (int)sizeof(after)) > 0);
    CHECK((unsigned char)after[0] == 0xEF);
    CHECK((unsigned char)after[1] == 0xBB);
    CHECK((unsigned char)after[2] == 0xBF);
    CHECK(strstr(after, "#ur0:tai/kubridge.skprx") != NULL);

    /* A path before the first section is a warning, not a lock. */
    fake_reset();
    fake_add_file("ur0:tai/config.txt",
                  "ur0:tai/stray.skprx\n*KERNEL\nur0:tai/kubridge.skprx\n");
    editor_scan();
    CHECK(g_editor.invalid == 1);
    CHECK(g_editor.read_only == 0);
    CHECK(g_editor.row_count == 2);
    int stray = editor_row_for_path("ur0:tai/stray.skprx");
    row = editor_row_for_path("ur0:tai/kubridge.skprx");
    CHECK(g_editor.rows[stray].status == EDITOR_NOT_REACHED);
    CHECK(editor_row_actionable(stray) == 0);
    CHECK(editor_row_actionable(row) == 1);
    CHECK(editor_toggle(row, 0, err, (int)sizeof(err)) == 0);

    /* A prose comment ending in a plugin suffix is not a row. */
    fake_reset();
    fake_add_file("ur0:tai/config.txt",
                  "# see kubridge.skprx for the bridge\n*KERNEL\n"
                  "ur0:tai/kubridge.skprx\n");
    editor_scan();
    CHECK(g_editor.row_count == 1);
}

static void test_case_and_boot(void)
{
    fake_reset();
    fake_add_file("ur0:tai/config.txt",
                  "*KERNEL\nur0:tai/KUBRIDGE.SKPRX\n");
    editor_scan();
    int row = editor_row_for_path("ur0:tai/kubridge.skprx");
    CHECK(row >= 0);
    CHECK(g_editor.rows[row].status == EDITOR_MISSING);
    CHECK(g_editor.rows[row].name[0] != '\0');
}

static void test_orphans(void)
{
    fake_reset();
    fake_add_file("ur0:tai/zzz.skprx", "");
    fake_add_file("ur0:tai/aaa.skprx", "");
    editor_scan();

    CHECK(!g_editor.orphans_unverified);
    CHECK(g_editor.orphan_count == 3);
    CHECK(strcmp(g_editor.orphans[0], "ur0:tai/aaa.skprx") == 0);
    CHECK(strcmp(g_editor.orphans[1], "ur0:tai/extra.skprx") == 0);
    CHECK(strcmp(g_editor.orphans[2], "ur0:tai/zzz.skprx") == 0);

    for (int i = 0; i < g_editor.orphan_count; ++i) {
        CHECK(!tai_ends_with(g_editor.orphans[i], ".original"));
        CHECK(!tai_ends_with(g_editor.orphans[i], ".session"));
        CHECK(!tai_ends_with(g_editor.orphans[i], ".bak"));
    }

    /* A missing ux0:tai directory is an empty listing, not an error. */
    CHECK(!platform_file_exists("ux0:tai"));
    CHECK(!g_editor.orphans_unverified);

    /* More files than the section can show is stated. */
    fake_reset();
    for (int i = 0; i < 40; ++i) {
        char path[64];
        snprintf(path, sizeof(path), "ur0:tai/overflow%02d.skprx", i);
        fake_add_file(path, "");
    }
    editor_scan();
    CHECK(g_editor.orphan_count == EDITOR_ORPHANS_MAX);
    CHECK(g_editor.orphan_truncated == 1);
}

static void test_ux0_precedence_and_empty(void)
{
    fake_reset();
    fake_add_file("ux0:tai/config.txt", "*main\nux0:tai/kubridge.skprx\n");
    fake_add_file("ux0:tai/kubridge.skprx", "");
    editor_scan();

    CHECK(strcmp(g_editor.source, "ux0:tai/config.txt") == 0);
    CHECK(strcmp(g_editor.ignored, "ur0:tai/config.txt") == 0);
    CHECK(g_editor.row_count == 1);
    CHECK(editor_row_for_path("ux0:tai/kubridge.skprx") >= 0);
    CHECK(editor_row_for_path("ur0:tai/kubridge.skprx") < 0);
    CHECK(text_is("ux0:tai/config.txt.session",
                  "*main\nux0:tai/kubridge.skprx\n"));

    fake_reset();
    fake_add_file("ux0:tai/config.txt", "");
    editor_scan();

    CHECK(strcmp(g_editor.source, "ux0:tai/config.txt") == 0);
    CHECK(g_editor.row_count == 0);
    CHECK(g_editor.valid == 1);
    CHECK(g_editor.read_only == 0);
    CHECK(text_is("ux0:tai/config.txt.session", ""));
}

static void test_input_edges(void)
{
    InputState s;
    input_reset(&s);

    CHECK(input_update(&s, 0, 0, 0, 0, 1, 0, 0, 0.016f) == INPUT_ACCEPT);
    CHECK(input_update(&s, 0, 0, 0, 0, 1, 0, 0, 0.016f) == INPUT_NONE);
    CHECK(input_update(&s, 0, 0, 0, 0, 0, 0, 0, 0.016f) == INPUT_NONE);

    input_reset(&s);
    CHECK(input_update(&s, 0, 0, 1, 0, 0, 0, 0, 0.016f) == INPUT_TAB_LEFT);
    CHECK(input_update(&s, 0, 0, 1, 0, 0, 0, 0, 0.016f) == INPUT_NONE);

    input_reset(&s);
    CHECK(input_update(&s, 0, 1, 0, 0, 0, 0, 0, 0.016f) == INPUT_DOWN);
    CHECK(input_update(&s, 0, 1, 0, 0, 0, 0, 0, 0.500f) == INPUT_DOWN);
    CHECK(input_update(&s, 0, 1, 0, 0, 0, 0, 0, 0.090f) == INPUT_DOWN);

    input_reset(&s);
    CHECK(input_update(&s, 0, 0, 0, 0, 1, 0, 0, 0.016f) == INPUT_ACCEPT);
    CHECK(input_update(&s, 0, 0, 0, 0, 1, 0, 1, 0.016f) == INPUT_NONE);
    CHECK(input_update(&s, 0, 0, 0, 0, 0, 0, 1, 0.016f) == INPUT_NONE);
    CHECK(input_update(&s, 0, 0, 0, 0, 1, 0, 1, 0.016f) == INPUT_ACCEPT);
}

int main(void)
{
    test_scan_sample();
    test_toggle_round_trip();
    test_hash_exactness();
    test_backups();
    test_write_failures();
    test_safety_refusals();
    test_statuses();
    test_bom_and_invalid();
    test_case_and_boot();
    test_orphans();
    test_ux0_precedence_and_empty();
    test_input_edges();

    if (g_failures != 0) {
        fprintf(stderr, "%d checks, %d failure(s)\n", g_checks, g_failures);
        return 1;
    }

    printf("%d checks, 0 failure(s)\n", g_checks);
    return 0;
}
