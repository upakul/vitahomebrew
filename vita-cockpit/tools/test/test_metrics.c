#include <stdio.h>
#include <string.h>

#include "core/history.h"
#include "core/metrics.h"
#include "core/platform.h"
#include "core/plugins.h"
#include "fake_platform.h"

/*
 * Host tests for the sampling policy.
 *
 * This is the part of COCKPIT that can be exercised without a console, which is
 * the whole reason core/platform.h exists (D7). Until these existed, the alert
 * policy was executed by nothing: deleting every arm of update_alert() left the
 * preview PNGs byte-identical (review finding #1).
 *
 * Not linked into cockpit.self. Run with tools/test/run.sh.
 */

#define GB (1024ull * 1024ull * 1024ull)
#define MB (1024ull * 1024ull)

static int g_checks;
static int g_failures;

#define CHECK(cond)                                                      \
    do {                                                                 \
        ++g_checks;                                                      \
        if (!(cond)) {                                                   \
            ++g_failures;                                                \
            printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);       \
        }                                                                \
    } while (0)

/* The hazard band is the only failure signal the app has (D15, D23). */
static void test_alert_policy(void)
{
    fake_reset();
    fake_set_power(20, 0);
    metrics_init();
    CHECK(g_metrics.alert == 0);

    fake_reset();
    fake_set_power(19, 0);
    metrics_init();
    CHECK(g_metrics.alert == 1);

    fake_reset();
    fake_set_power(19, 1);
    metrics_init();
    CHECK(g_metrics.alert == 0);

    fake_reset();
    fake_set_storage_absent(PLATFORM_STORAGE_UX0);
    metrics_init();
    CHECK(g_metrics.alert == 1);

    fake_reset();
    fake_set_memory_absent();
    metrics_init();
    CHECK(g_metrics.alert == 1);

    /* A failed power read is its own fault, not a silent all-clear (D23). */
    fake_reset();
    fake_set_power_absent();
    metrics_init();
    CHECK(g_metrics.alert == 1);
}

/* A failed read must not enter the trace (review finding #2). */
static void test_failed_memory_leaves_trace_alone(void)
{
    fake_reset();
    metrics_init();

    for (int i = 0; i < 5; ++i) {
        fake_set_memory_mb(180 + i);
        metrics_sample_slow();
    }

    int len_before = g_metrics.hist_mem.len;
    CHECK(len_before > 0);
    CHECK(g_metrics.mem_valid == 1);

    fake_set_memory_absent();
    metrics_sample_slow();

    CHECK(g_metrics.mem_valid == 0);
    CHECK(g_metrics.hist_mem.len == len_before);
}

/* Unit conversion: the [C] storage risk, now pinned. */
static void test_storage_conversion(void)
{
    fake_reset();
    fake_set_storage(PLATFORM_STORAGE_UX0, 32ull * GB, 20ull * GB);
    metrics_init();

    CHECK(g_metrics.storage[PLATFORM_STORAGE_UX0].valid == 1);
    CHECK(g_metrics.storage[PLATFORM_STORAGE_UX0].total_mb == 32768);
    CHECK(g_metrics.storage[PLATFORM_STORAGE_UX0].used_mb == 12288);

    /* A sub-gigabyte volume keeps its MB precision, so the panel can print
     * "384/512 MB" instead of a rounded "0.4/0.5 GB". */
    fake_reset();
    fake_set_storage(PLATFORM_STORAGE_UX0, 512ull * MB, 128ull * MB);
    metrics_init();
    CHECK(g_metrics.storage[PLATFORM_STORAGE_UX0].total_mb == 512);
    CHECK(g_metrics.storage[PLATFORM_STORAGE_UX0].used_mb == 384);

    /* Absent mount is absent, not zero. */
    fake_reset();
    fake_set_storage_absent(PLATFORM_STORAGE_UMA0);
    metrics_init();
    CHECK(g_metrics.storage[PLATFORM_STORAGE_UMA0].valid == 0);
}

static void test_memory_conversion(void)
{
    fake_reset();
    fake_set_memory_mb(182);
    metrics_init();

    CHECK(g_metrics.mem_valid == 1);
    CHECK(g_metrics.mem_user_mb == 182);
    CHECK(g_metrics.mem_cdram_mb == 41);
    CHECK(g_metrics.mem_phy_mb == 64);
}

/* The axis is bracketed by the window's own samples, not by zero. */
static void test_mem_axis(void)
{
    fake_reset();
    metrics_init();

    for (int i = 0; i < 5; ++i) {
        fake_set_memory_mb(150 + i * 10);
        metrics_sample_slow();
    }

    CHECK(g_metrics.hist_mem_lo == 150.0f);
    CHECK(g_metrics.hist_mem_hi == 190.0f);

    /* A flat window is padded around the sample, not from zero. */
    fake_reset();
    metrics_init();
    fake_set_memory_mb(182);
    metrics_sample_slow();

    CHECK(g_metrics.hist_mem_lo == 180.0f);
    CHECK(g_metrics.hist_mem_hi == 184.0f);
    CHECK((g_metrics.hist_mem_lo + g_metrics.hist_mem_hi) * 0.5f == 182.0f);
}

static void test_history_ring(void)
{
    History h;

    history_reset(&h);
    CHECK(h.len == 0);
    CHECK(history_at(&h, 0) == 0.0f);

    for (int i = 0; i < 5; ++i)
        history_push(&h, (float)i);

    CHECK(h.len == 5);
    CHECK(history_at(&h, 0) == 4.0f); /* newest first */
    CHECK(history_at(&h, 4) == 0.0f);

    for (int i = 5; i < HISTORY_CAP + 3; ++i)
        history_push(&h, (float)i);

    CHECK(h.len == HISTORY_CAP);
    CHECK(history_at(&h, 0) == (float)(HISTORY_CAP + 2));
    CHECK(history_at(&h, HISTORY_CAP) == 0.0f); /* out of range */
}

static void test_frame_floor(void)
{
    fake_reset();
    metrics_init();
    metrics_sample_frame(0.0f);
    float zero_dt = g_metrics.frame_ms;

    /* A negative dt is treated exactly as zero, not as a huge frame. */
    fake_reset();
    metrics_init();
    metrics_sample_frame(-1.0f);
    CHECK(g_metrics.frame_ms == zero_dt);

    /* The floor holds after enough zero-dt frames to decay the EMA to nothing. */
    fake_reset();
    metrics_init();
    for (int i = 0; i < 200; ++i)
        metrics_sample_frame(0.0f);
    CHECK(g_metrics.frame_ms >= 0.1f);

    /* A 33 ms frame is the peak the FRAME trace shows, not the EMA. */
    fake_reset();
    metrics_init();
    metrics_sample_frame(0.033f);
    metrics_sample_slow();
    CHECK(history_at(&g_metrics.hist_frame, 0) >= 33.0f);
}

/* tai config parsing: "listed" and "loaded" are different questions. */
static void test_plugins_parse(void)
{
    static const char CFG[] = "# comment\n"
                              "*KERNEL\n"
                              "ur0:tai/kubridge.skprx\n"
                              "  ux0:tai/nonpdrm.skprx  \r\n"
                              "*main\n"
                              "ur0:tai/libshacccg.suprx\n"
                              "#ur0:tai/vitaos_ps.suprx\n"
                              "not-a-plugin.txt\n"
                              "ur0:tai/kubridge.skprx\n";

    PluginList list;
    memset(&list, 0, sizeof(list));

    int added = plugins_parse(CFG, &list);

    CHECK(added == 3);
    CHECK(list.count == 3);

    CHECK(list.items[0].section == PLUGIN_SECTION_KERNEL);
    CHECK(list.items[1].section == PLUGIN_SECTION_KERNEL);
    CHECK(list.items[2].section == PLUGIN_SECTION_SYSTEM);

    /* A commented-out entry is deactivated, not merely unreadable. */
    for (int i = 0; i < list.count; ++i)
        CHECK(strcmp(list.items[i].name, "vitaos_ps.suprx") != 0);
    CHECK(strcmp(list.items[0].name, "kubridge.skprx") == 0);
    CHECK(strcmp(list.items[1].name, "nonpdrm.skprx") == 0);
    CHECK(strcmp(list.items[2].name, "libshacccg.suprx") == 0);

    /* The full path is kept, because existence is checked against it. */
    CHECK(strcmp(list.items[0].path, "ur0:tai/kubridge.skprx") == 0);
    CHECK(strcmp(list.items[1].path, "ux0:tai/nonpdrm.skprx") == 0);

    /* Known plugins carry a description; unknown ones keep an empty one rather
     * than an invented one. Check every known row, not just the first: a
     * dropped table entry is silent at runtime and must fail here. */
    CHECK(list.items[0].desc[0] != '\0');
    CHECK(list.items[1].desc[0] != '\0');
    CHECK(list.items[2].desc[0] != '\0');

    PluginList unknown;
    memset(&unknown, 0, sizeof(unknown));
    plugins_parse("*main\nur0:tai/mystery.skprx\n", &unknown);
    CHECK(unknown.count == 1);
    CHECK(unknown.items[0].desc[0] == '\0');

    /* The files this console actually lists must resolve, not just the catalog
     * entries: a typo in a key leaves a real plugin with no description. */
    static const char *const NAMED[] = {"yamt.suprx", "henkaku.suprx",
                                        "vitabtfix.skprx",
                                        "storagemgr.skprx"};

    for (size_t i = 0; i < sizeof(NAMED) / sizeof(NAMED[0]); ++i) {
        char cfg[64];
        PluginList one;

        memset(&one, 0, sizeof(one));
        snprintf(cfg, sizeof(cfg), "*main\nur0:tai/%s\n", NAMED[i]);
        plugins_parse(cfg, &one);
        CHECK(one.count == 1);
        CHECK(one.items[0].desc[0] != '\0');
    }

    /* A header line is not a plugin, and an empty config yields nothing. */
    PluginList empty;
    memset(&empty, 0, sizeof(empty));
    plugins_parse("*KERNEL\n*main\n", &empty);
    CHECK(empty.count == 0);
}

/* The section a named plugin was filed under, or -1 when it is absent. */
static int section_of(const PluginList *list, const char *name)
{
    for (int i = 0; i < list->count; ++i) {
        if (strcmp(list->items[i].name, name) == 0)
            return (int)list->items[i].section;
    }

    return -1;
}

/* Section headers decide the group. A file listed under several targets is
 * shown once, under the most privileged one; a path before any header is not
 * loaded and must not appear (D51). */
static void test_plugins_sections(void)
{
    static const char CFG[] = "ur0:tai/orphan.skprx\n"
                              "*NPXS10015\n"
                              "ur0:tai/settings.suprx\n"
                              "ur0:tai/shared.suprx\n"
                              "*KERNEL\n"
                              "ur0:tai/kern.skprx\n"
                              "ur0:tai/shared.suprx\n"
                              "*main\n"
                              "ur0:tai/shell.suprx\n"
                              "*NPXS10016\n"
                              "ur0:tai/widget.suprx\n"
                              "*all\n"
                              "ur0:tai/all.suprx\n"
                              "*PCSE00001\n"
                              "ur0:tai/game.suprx\n";

    PluginList list;
    memset(&list, 0, sizeof(list));
    plugins_parse(CFG, &list);

    CHECK(list.count == 7);
    CHECK(section_of(&list, "orphan.skprx") == -1);
    CHECK(section_of(&list, "settings.suprx") == PLUGIN_SECTION_SYSTEM_APP);
    CHECK(section_of(&list, "kern.skprx") == PLUGIN_SECTION_KERNEL);
    CHECK(section_of(&list, "shared.suprx") == PLUGIN_SECTION_KERNEL);
    CHECK(section_of(&list, "shell.suprx") == PLUGIN_SECTION_SYSTEM);
    CHECK(section_of(&list, "widget.suprx") == PLUGIN_SECTION_SYSTEM_APP);
    CHECK(section_of(&list, "all.suprx") == PLUGIN_SECTION_ALL_TITLES);
    CHECK(section_of(&list, "game.suprx") == PLUGIN_SECTION_TITLE);
}

/*
 * A real config lists more than a dozen plugins. The array used to hold 12, so
 * the tail of config.txt vanished without any sign; the ledger scrolls, so the
 * number of rows is a storage question, not a layout one.
 */
static void test_plugins_capacity(void)
{
    enum { N = 40 };
    char cfg[N * 32 + 16];
    size_t off = 0;

    off += (size_t)snprintf(cfg + off, sizeof(cfg) - off, "*KERNEL\n");

    for (int i = 0; i < N; ++i)
        off += (size_t)snprintf(cfg + off, sizeof(cfg) - off,
                                "ur0:tai/plugin%02d.skprx\n", i);

    PluginList list;
    memset(&list, 0, sizeof(list));

    int added = plugins_parse(cfg, &list);

    CHECK(added == N);
    CHECK(list.count == N);
    CHECK(list.truncated == 0);
    CHECK(strcmp(list.items[0].name, "plugin00.skprx") == 0);
    CHECK(strcmp(list.items[N - 1].name, "plugin39.skprx") == 0);
    CHECK(strcmp(list.items[N - 1].path, "ur0:tai/plugin39.skprx") == 0);
    CHECK(list.items[N - 1].section == PLUGIN_SECTION_KERNEL);
}

/* The longest catalog file name is 41 characters. A 32-byte field cut the name
 * and made its description lookup miss. */
static void test_plugins_long_name(void)
{
    static const char PATH[] =
        "ur0:tai/analogstickdisable_swapleftandright.skprx";
    char cfg[sizeof(PATH) + 8];
    PluginList list;

    memset(&list, 0, sizeof(list));
    snprintf(cfg, sizeof(cfg), "*main\n%s\n", PATH);
    plugins_parse(cfg, &list);

    CHECK(list.count == 1);
    CHECK(strcmp(list.items[0].name,
                 "analogstickdisable_swapleftandright.skprx") == 0);
    CHECK(strcmp(list.items[0].path, PATH) == 0);
    CHECK(list.items[0].desc[0] != '\0');
}

/* If the ceiling is ever reached the loss is recorded, so the panel can say so
 * rather than presenting a short list as complete. */
static void test_plugins_truncation(void)
{
    char cfg[(PLUGINS_MAX + 2) * 32];
    size_t off = 0;

    off += (size_t)snprintf(cfg + off, sizeof(cfg) - off, "*KERNEL\n");

    for (int i = 0; i < PLUGINS_MAX + 1; ++i)
        off += (size_t)snprintf(cfg + off, sizeof(cfg) - off,
                                "ur0:tai/plugin%02d.skprx\n", i);

    PluginList list;
    memset(&list, 0, sizeof(list));
    plugins_parse(cfg, &list);

    CHECK(list.count == PLUGINS_MAX);
    CHECK(list.truncated == 1);

    /* A repeated line after the ceiling loses nothing, so it must not raise the
     * flag: config.txt can list henkaku.suprx in several sections. */
    PluginList full;
    memset(&full, 0, sizeof(full));
    off = 0;

    off += (size_t)snprintf(cfg + off, sizeof(cfg) - off, "*KERNEL\n");

    for (int i = 0; i < PLUGINS_MAX; ++i)
        off += (size_t)snprintf(cfg + off, sizeof(cfg) - off,
                                "ur0:tai/plugin%02d.skprx\n", i);

    plugins_parse(cfg, &full);
    plugins_parse("ur0:tai/plugin00.skprx\n", &full);

    CHECK(full.count == PLUGINS_MAX);
    CHECK(full.truncated == 0);
}

int main(void)
{
    test_alert_policy();
    test_failed_memory_leaves_trace_alone();
    test_storage_conversion();
    test_memory_conversion();
    test_mem_axis();
    test_history_ring();
    test_frame_floor();
    test_plugins_parse();
    test_plugins_sections();
    test_plugins_capacity();
    test_plugins_long_name();
    test_plugins_truncation();

    printf("%d checks, %d failure(s)\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
