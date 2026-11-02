#include "core/metrics.h"

#include <string.h>

/*
 * Sampling policy. This file contains no SDK call and no draw call: every
 * reading arrives through core/platform.h, which is what lets it be compiled
 * and exercised on a host against a fake platform (D7).
 *
 * The one writer of g_metrics (D16).
 */

#define BYTES_PER_MB (1024ull * 1024ull)

/* Ceiling on a reported capacity, so a misread reply cannot overflow the int
 * math in the panel's unit formatting. 1 TiB is far beyond any Vita volume. */
#define STORAGE_MB_MAX 1000000

#define FRAME_EMA_ALPHA 0.05f
#define FRAME_MS_FLOOR 0.1f
#define FRAME_MS_SEED 16.667f

Metrics g_metrics;

static float s_ema_ms = FRAME_MS_SEED;

/*
 * Worst frame since the last slow sample. The FRAME strip's axis is fixed at
 * two frame periods, and a sample reaching the top is meant to mean "a frame
 * was dropped" -- which the EMA can never show, because one 33 ms frame moves a
 * 16.7 ms baseline by under a pixel (review finding #12).
 */
static float s_peak_ms;

static void sample_clocks(void)
{
    PlatformClocks in;
    platform_read_clocks(&in);

    g_metrics.clocks_valid = in.ok;
    if (!in.ok)
        return;

    g_metrics.arm_mhz = in.arm_mhz;
    g_metrics.gpu_mhz = in.gpu_mhz;
    g_metrics.bus_mhz = in.bus_mhz;
}

static void sample_memory(void)
{
    PlatformMemory in;
    platform_read_memory(&in);

    g_metrics.mem_valid = in.ok;
    if (!in.ok)
        return;

    g_metrics.mem_user_mb = (int)(in.user_bytes / BYTES_PER_MB);
    g_metrics.mem_cdram_mb = (int)(in.cdram_bytes / BYTES_PER_MB);
    g_metrics.mem_phy_mb = (int)(in.phycont_bytes / BYTES_PER_MB);
}

static void sample_storage(int i)
{
    PlatformStorage in;
    platform_read_storage(i, &in);

    g_metrics.storage[i].error = in.error;
    g_metrics.storage[i].errno_byte = in.errno_byte;
    g_metrics.storage[i].valid = in.ok;
    if (!in.ok)
        return;

    uint64_t total_mb = in.total_bytes / BYTES_PER_MB;
    uint64_t used_mb = (in.total_bytes - in.free_bytes) / BYTES_PER_MB;

    if (total_mb > STORAGE_MB_MAX)
        total_mb = STORAGE_MB_MAX;
    if (used_mb > total_mb)
        used_mb = total_mb;

    g_metrics.storage[i].total_mb = (int)total_mb;
    g_metrics.storage[i].used_mb = (int)used_mb;
}

static void sample_power(void)
{
    PlatformPower in;
    platform_read_power(&in);

    g_metrics.power_valid = in.ok;
    if (!in.ok)
        return;

    g_metrics.batt_pct = in.percent;
    g_metrics.batt_temp_c = in.temp_c;
    g_metrics.batt_mv = in.millivolts;
    g_metrics.batt_cycles = in.cycles;
    g_metrics.batt_health = in.health;
    g_metrics.charging = in.charging;
}

static void sample_identity(void)
{
    PlatformIdentity in;
    platform_read_identity(&in);

    g_metrics.model = in.model;
    g_metrics.fw_valid = in.fw_ok;

    if (in.fw_ok) {
        memcpy(g_metrics.fw, in.fw, FW_STRING_MAX);
        g_metrics.fw[FW_STRING_MAX - 1] = '\0';
    }
}

static void sample_clock(void)
{
    PlatformClock in;
    platform_read_clock(&in);

    g_metrics.clock_valid = in.ok;
    if (!in.ok)
        return;

    g_metrics.clock_h = in.hour;
    g_metrics.clock_m = in.minute;
    g_metrics.clock_s = in.second;
}

static void update_alert(void)
{
    int a = 0;

    if (!g_metrics.mem_valid)
        a = 1;

    /*
     * A failed power read is its own fault. Without this the band would clear
     * the moment scePowerGetBatteryLifePercent() started returning a negative
     * value, silently dropping an alert that was already true (D23).
     */
    if (!g_metrics.power_valid)
        a = 1;

    if (g_metrics.power_valid && g_metrics.batt_pct < LOW_BATTERY_PCT &&
        !g_metrics.charging)
        a = 1;

    if (!g_metrics.storage[PLATFORM_STORAGE_UX0].valid)
        a = 1;

    g_metrics.alert = a;
}

/* Axis range for the CORE trace, padded so a flat line sits mid-box. */
static void update_mem_axis(void)
{
    float lo;
    float hi;

    history_range(&g_metrics.hist_mem, &lo, &hi);

    if (hi - lo < HIST_MEM_MIN_SPAN) {
        float mid = (lo + hi) * 0.5f;
        lo = mid - HIST_MEM_MIN_SPAN * 0.5f;
        hi = mid + HIST_MEM_MIN_SPAN * 0.5f;
    }

    g_metrics.hist_mem_lo = lo;
    g_metrics.hist_mem_hi = hi;
}

void metrics_init(void)
{
    memset(&g_metrics, 0, sizeof(g_metrics));
    history_reset(&g_metrics.hist_mem);
    history_reset(&g_metrics.hist_frame);

    s_ema_ms = FRAME_MS_SEED;
    s_peak_ms = 0.0f;
    g_metrics.frame_ms = s_ema_ms;
    g_metrics.fps = 1000.0f / s_ema_ms;

    metrics_sample_slow();
}

void metrics_sample_slow(void)
{
    sample_clocks();
    sample_memory();

    for (int i = 0; i < STORAGE_COUNT; ++i)
        sample_storage(i);

    sample_power();
    sample_identity();
    sample_clock();

    g_metrics.uptime_s = platform_uptime_seconds();

    /*
     * Only a real reading enters the trace. Pushing on a failed sample would
     * draw the previous value as if the console still reported it, which is
     * exactly what PROJECT.md section 6 forbids.
     */
    if (g_metrics.mem_valid)
        history_push(&g_metrics.hist_mem, (float)g_metrics.mem_user_mb);

    history_push(&g_metrics.hist_frame, s_peak_ms);
    s_peak_ms = 0.0f;

    update_mem_axis();
    update_alert();
}

void metrics_sample_frame(float dt)
{
    float ms = dt * 1000.0f;
    if (ms < 0.0f)
        ms = 0.0f;

    s_ema_ms += (ms - s_ema_ms) * FRAME_EMA_ALPHA;
    if (s_ema_ms < FRAME_MS_FLOOR)
        s_ema_ms = FRAME_MS_FLOOR;

    g_metrics.frame_ms = s_ema_ms;
    g_metrics.fps = 1000.0f / s_ema_ms;

    if (ms > s_peak_ms)
        s_peak_ms = ms;
}
