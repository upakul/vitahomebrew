#include <stdio.h>
#include <string.h>

#include "core/input.h"
#include "core/plugin_editor.h"
#include "core/platform.h"
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

static int text_is(const char *path, const char *expected)
{
    char buf[32768];
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

    int henkaku = editor_row_for_path("ur0:tai/henkaku.suprx");
    CHECK(henkaku >= 0);
    CHECK(g_editor.rows[henkaku].locked == 1);
    CHECK(editor_next_action(henkaku) == -1);

    int storage = editor_row_for_path("ur0:tai/storagemgr.skprx");
    CHECK(storage >= 0);
    CHECK(editor_confirm_text(storage, 0) != NULL);
    CHECK(editor_confirm_text(storage, 1) != NULL);

    CHECK(editor_confirm_text(editor_row_for_path("ur0:tai/kubridge.skprx"), 1) == NULL);

    int shellbat = editor_row_for_path("ur0:tai/shellbat.suprx");
    CHECK(shellbat >= 0);
    CHECK(g_editor.rows[shellbat].status == EDITOR_DISABLED);
    CHECK(editor_next_action(shellbat) == 1);
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

    char err[64];
    CHECK(editor_toggle(row, 0, err, (int)sizeof(err)) == 0);
    CHECK(g_editor.rows[row].status == EDITOR_DISABLED);
    CHECK(g_editor.pending_count == 1);
    CHECK(text_is("ur0:tai/config.txt.bak", "# sample tai config\n"
                                            "*KERNEL\n"
                                            "ur0:tai/kubridge.skprx\n"
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
    CHECK(g_editor.rows[row].status == EDITOR_ENABLED);
    CHECK(g_editor.pending_count == 0);

    char after[32768];
    CHECK(read_file("ur0:tai/config.txt", after, (int)sizeof(after)) > 0);
    CHECK(strcmp(before, after) == 0);
}

static void test_backup_rotation(void)
{
    fake_reset();
    fake_add_file("ur0:tai/config.txt.bak", "session-one");
    editor_scan();

    CHECK(text_is("ur0:tai/config.txt.1.bak", "session-one"));

    editor_scan();
    CHECK(text_is("ur0:tai/config.txt.2.bak", "session-one"));
    CHECK(text_is("ur0:tai/config.txt.1.bak",
                  "# sample tai config\n"
                  "*KERNEL\n"
                  "ur0:tai/kubridge.skprx\n"
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
}

static void test_write_and_backup_failure(void)
{
    fake_reset();
    editor_scan();

    int row = editor_row_for_path("ur0:tai/kubridge.skprx");
    char err[64];

    fake_fail_write(1);
    CHECK(editor_toggle(row, 0, err, (int)sizeof(err)) != 0);
    CHECK(g_editor.rows[row].status == EDITOR_ENABLED);
    CHECK(g_editor.pending_count == 0);

    fake_fail_write(0);
    fake_fail_backup(1);
    editor_scan();
    CHECK(g_editor.read_only == 1);
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
    CHECK(g_editor.orphan_count > 0);
    CHECK(g_editor.orphan_shadowed[0] == 1);

    fake_reset();
    fake_add_file("ux0:tai/config.txt", "");
    editor_scan();

    CHECK(strcmp(g_editor.source, "ux0:tai/config.txt") == 0);
    CHECK(g_editor.row_count == 0);
    CHECK(g_editor.valid == 1);
}

static void test_duplicate_basename_and_halt(void)
{
    fake_reset();
    fake_add_file("ur0:tai/config.txt",
                  "*main\nur0:tai/kubridge.skprx\nux0:tai/kubridge.skprx\n");
    fake_add_file("ux0:tai/kubridge.skprx", "");
    editor_scan();

    CHECK(g_editor.row_count == 2);
    CHECK(editor_row_for_path("ur0:tai/kubridge.skprx") >= 0);
    CHECK(editor_row_for_path("ux0:tai/kubridge.skprx") >= 0);
    CHECK(strcmp(g_editor.rows[0].name, g_editor.rows[1].name) == 0);

    fake_reset();
    fake_add_file("ur0:tai/config.txt",
                  "*KERNEL\nur0:tai/kubridge.skprx\n*!ALL\n"
                  "ur0:tai/extra.skprx\n");
    editor_scan();

    int reached = editor_row_for_path("ur0:tai/kubridge.skprx");
    int halted = editor_row_for_path("ur0:tai/extra.skprx");
    CHECK(reached >= 0);
    CHECK(halted >= 0);
    CHECK(g_editor.rows[reached].status == EDITOR_ENABLED);
    CHECK(g_editor.rows[halted].status == EDITOR_NOT_REACHED);
    CHECK(editor_next_action(halted) == -1);
    CHECK(g_editor.activated_total == 1);
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

    fake_reset();
    editor_scan();
    fake_add_file("ur0:tai/config.txt.bak", "different");
    CHECK(editor_toggle(row, 0, err, (int)sizeof(err)) != 0);
    CHECK(strstr(err, "BACKUP MISSING") != NULL);
}

static void test_statuses_and_partial_cycle(void)
{
    fake_reset();
    fake_add_file("ur0:tai/config.txt",
                  "*main\nur0:tai/kubridge.skprx\n"
                  "*NPXS10015\n#ur0:tai/kubridge.skprx\n");
    editor_scan();

    int row = editor_row_for_path("ur0:tai/kubridge.skprx");
    CHECK(row >= 0);
    CHECK(g_editor.rows[row].status == EDITOR_PARTIAL);
    CHECK(editor_next_action(row) == 0);
    CHECK(editor_toggle(row, 0, NULL, 0) == 0);
    CHECK(g_editor.rows[row].status == EDITOR_DISABLED);
    CHECK(editor_toggle(row, 1, NULL, 0) == 0);
    CHECK(g_editor.rows[row].status == EDITOR_ENABLED);
    CHECK(editor_toggle(row, 2, NULL, 0) == 0);
    CHECK(g_editor.rows[row].status == EDITOR_PARTIAL);
    CHECK(g_editor.pending_count == 0);

    fake_reset();
    fake_add_file("ur0:tai/config.txt", "*KERNEL\nur0:tai/taihen.skprx\n");
    editor_scan();
    row = editor_row_for_path("ur0:tai/taihen.skprx");
    CHECK(row >= 0);
    CHECK(g_editor.rows[row].status == EDITOR_BOOT);
    CHECK(editor_next_action(row) == -1);

    fake_reset();
    fake_add_file("ur0:tai/config.txt", "*KERNEL\nur0:tai/absent.skprx\n");
    editor_scan();
    row = editor_row_for_path("ur0:tai/absent.skprx");
    CHECK(row >= 0);
    CHECK(g_editor.rows[row].status == EDITOR_MISSING);
    CHECK(editor_next_action(row) == 0);
    CHECK(editor_toggle(row, 1, NULL, 0) != 0);

    /* Display order is section-major, so a *KERNEL row listed second sorts
     * above an earlier *main row and the cursor follows the drawn order. */
    fake_reset();
    fake_add_file("ur0:tai/config.txt",
                  "*main\nur0:tai/z.suprx\n*KERNEL\nur0:tai/a.skprx\n");
    fake_add_file("ur0:tai/z.suprx", "");
    fake_add_file("ur0:tai/a.skprx", "");
    editor_scan();
    CHECK(strcmp(g_editor.rows[0].name, "a.skprx") == 0);
    CHECK(g_editor.rows[0].section == PLUGIN_SECTION_KERNEL);
    CHECK(editor_row_for_path("ur0:tai/a.skprx") == 0);
}

static void test_long_config_is_read_only(void)
{
    enum { N = EDITOR_LINES_MAX + 1 };
    char cfg[N * 24 + 32];
    size_t off = 0;

    fake_reset();
    off += (size_t)snprintf(cfg + off, sizeof(cfg) - off, "*KERNEL\n");
    for (int i = 0; i < N; ++i)
        off += (size_t)snprintf(cfg + off, sizeof(cfg) - off,
                                "ur0:tai/p%03d.skprx\n", i);
    fake_add_file("ur0:tai/config.txt", cfg);
    editor_scan();

    CHECK(g_editor.lines_truncated == 1);
    CHECK(g_editor.read_only == 1);
    CHECK(editor_row_actionable(0) == 0);
    CHECK(editor_toggle(0, 0, NULL, 0) != 0);
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
    test_backup_rotation();
    test_write_and_backup_failure();
    test_safety_refusals();
    test_statuses_and_partial_cycle();
    test_long_config_is_read_only();
    test_ux0_precedence_and_empty();
    test_duplicate_basename_and_halt();
    test_input_edges();

    if (g_failures != 0) {
        fprintf(stderr, "%d checks, %d failure(s)\n", g_checks, g_failures);
        return 1;
    }

    printf("%d checks, 0 failure(s)\n", g_checks);
    return 0;
}
