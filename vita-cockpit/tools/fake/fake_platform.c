#include "fake_platform.h"

#include <string.h>

#include "core/platform.h"

/*
 * Mutable fake platform. Never linked into cockpit.self.
 *
 * The default state is a plausible 3.65 Vita with an SD2Vita card. uma0: is
 * absent on purpose, so the ordinary preview exercises the "--" path as well as
 * the populated one.
 */

#define GB (1024ull * 1024ull * 1024ull)
#define MB (1024ull * 1024ull)

static int s_mem_ok;
static int s_mem_mb;

static int s_power_ok;
static int s_power_pct;
static int s_power_charging;

static int s_storage_ok[PLATFORM_STORAGE_COUNT];
static uint64_t s_storage_total[PLATFORM_STORAGE_COUNT];
static uint64_t s_storage_free[PLATFORM_STORAGE_COUNT];

static int s_identity_ok;
static int s_clock_ok;

void fake_reset(void)
{
    s_mem_ok = 1;
    s_mem_mb = 182;

    s_power_ok = 1;
    s_power_pct = 87;
    s_power_charging = 1;

    memset(s_storage_ok, 0, sizeof(s_storage_ok));
    memset(s_storage_total, 0, sizeof(s_storage_total));
    memset(s_storage_free, 0, sizeof(s_storage_free));

    s_storage_ok[PLATFORM_STORAGE_UX0] = 1;
    s_storage_total[PLATFORM_STORAGE_UX0] = 29ull * GB + GB / 2;
    s_storage_free[PLATFORM_STORAGE_UX0] =
        s_storage_total[PLATFORM_STORAGE_UX0] - (12ull * GB + 4 * GB / 10);

    /* uma0: absent, as it is on a console with no second volume. */

    s_identity_ok = 1;
    s_clock_ok = 1;
}

void fake_set_memory_mb(int user_mb)
{
    s_mem_ok = 1;
    s_mem_mb = user_mb;
}

void fake_set_memory_absent(void)
{
    s_mem_ok = 0;
}

void fake_set_power(int percent, int charging)
{
    s_power_ok = 1;
    s_power_pct = percent;
    s_power_charging = charging;
}

void fake_set_power_absent(void)
{
    s_power_ok = 0;
}

void fake_set_storage(int index, uint64_t total_bytes, uint64_t free_bytes)
{
    if (index < 0 || index >= PLATFORM_STORAGE_COUNT)
        return;

    s_storage_ok[index] = 1;
    s_storage_total[index] = total_bytes;
    s_storage_free[index] = free_bytes;
}

void fake_set_storage_absent(int index)
{
    if (index < 0 || index >= PLATFORM_STORAGE_COUNT)
        return;

    s_storage_ok[index] = 0;
}

void fake_set_identity_absent(void)
{
    s_identity_ok = 0;
}

void fake_set_clock_absent(void)
{
    s_clock_ok = 0;
}

void platform_read_storage(int index, PlatformStorage *out)
{
    if (out == NULL)
        return;

    memset(out, 0, sizeof(*out));

    if (index < 0 || index >= PLATFORM_STORAGE_COUNT)
        return;
    if (!s_storage_ok[index])
        return;

    out->total_bytes = s_storage_total[index];
    out->free_bytes = s_storage_free[index];
    out->ok = 1;
}

void platform_read_clocks(PlatformClocks *out)
{
    if (out == NULL)
        return;

    memset(out, 0, sizeof(*out));
    out->arm_mhz = 333;
    out->gpu_mhz = 166;
    out->bus_mhz = 166;
    out->ok = 1;
}

void platform_read_memory(PlatformMemory *out)
{
    if (out == NULL)
        return;

    memset(out, 0, sizeof(*out));

    if (!s_mem_ok)
        return;

    out->user_bytes = (uint64_t)s_mem_mb * MB;
    out->cdram_bytes = 41ull * MB;
    out->phycont_bytes = 64ull * MB;
    out->ok = 1;
}

void platform_read_power(PlatformPower *out)
{
    if (out == NULL)
        return;

    memset(out, 0, sizeof(*out));

    if (!s_power_ok)
        return;

    out->percent = s_power_pct;
    out->temp_c = 31;
    out->millivolts = 3820;
    out->cycles = 214;
    out->health = 98;
    out->charging = s_power_charging;
    out->ok = 1;
}

void platform_read_identity(PlatformIdentity *out)
{
    if (out == NULL)
        return;

    memset(out, 0, sizeof(*out));

    if (!s_identity_ok)
        return;

    out->model = PLATFORM_MODEL_VITA;
    memcpy(out->fw, "3.65", 5);
    out->fw_ok = 1;
}

void platform_read_clock(PlatformClock *out)
{
    if (out == NULL)
        return;

    memset(out, 0, sizeof(*out));

    if (!s_clock_ok)
        return;

    out->hour = 14;
    out->minute = 22;
    out->second = 7;
    out->ok = 1;
}

double platform_uptime_seconds(void)
{
    return 862.0;
}

/*
 * A representative tai config, so the preview renders the plugin strip with
 * real content instead of an empty state.
 */
static const char SAMPLE_CONFIG[] = "# sample tai config\n"
                                    "*KERNEL\n"
                                    "ur0:tai/kubridge.skprx\n"
                                    "ur0:tai/storagemgr.skprx\n"
                                    "*main\n"
                                    "ur0:tai/libshacccg.suprx\n"
                                    "ur0:tai/mystery.suprx\n";

int platform_read_text_file(const char *path, char *buf, int cap)
{
    if (path == NULL || buf == NULL || cap <= 1)
        return -1;

    if (strcmp(path, "ur0:tai/config.txt") != 0)
        return -1;

    int n = (int)strlen(SAMPLE_CONFIG);
    if (n >= cap)
        n = cap - 1;

    memcpy(buf, SAMPLE_CONFIG, (size_t)n);
    buf[n] = '\0';
    return n;
}

/*
 * The files the sample config lists. Answering from this table rather than
 * re-testing the suffix keeps the fake from restating core/plugins.c's rule,
 * where the two could drift.
 */
static const char *const PRESENT[] = {
    "ur0:tai/kubridge.skprx",
    "ur0:tai/storagemgr.skprx",
    "ur0:tai/libshacccg.suprx",
    "ur0:tai/mystery.suprx",
};

int platform_file_exists(const char *path)
{
    if (path == NULL)
        return 0;

    for (size_t i = 0; i < sizeof(PRESENT) / sizeof(PRESENT[0]); ++i) {
        if (strcmp(path, PRESENT[i]) == 0)
            return 1;
    }

    return 0;
}
