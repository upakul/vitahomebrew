#ifndef COCKPIT_CORE_METRICS_H
#define COCKPIT_CORE_METRICS_H

#include "core/history.h"
#include "core/platform.h"

#define STORAGE_COUNT PLATFORM_STORAGE_COUNT
#define FW_STRING_MAX PLATFORM_FW_MAX

/* Alert threshold, percent, discharging only. */
#define LOW_BATTERY_PCT 20

/* Smallest axis span the CORE trace will draw. Below this the line is flat and
 * the box would either pin it to an edge or divide by zero. */
#define HIST_MEM_MIN_SPAN 4.0f

/*
 * Storage in whole megabytes.
 *
 * MB rather than tenths of a GB so a small volume keeps its precision: a 512 MB
 * card is "512 MB", not "0.5 GB". The panel picks the unit from the magnitude.
 */
typedef struct {
    int used_mb;
    int total_mb;
    int error; /* the platform call's return code, 0 when it succeeded */
    unsigned errno_byte; /* low byte of `error`, already decoded by the port */
    int valid;
} StorageSample;

/*
 * One snapshot of the machine, already converted to the units the HUD speaks.
 *
 * Unit conversion happens here, not in a panel, so that "what a gigabyte means"
 * has one home. Panels format strings; they do not decide magnitudes.
 *
 * Every field has a matching `*_valid` flag. A panel must render "--" when the
 * flag is clear rather than printing the zero it happens to hold.
 */
typedef struct {
    int arm_mhz;
    int gpu_mhz;
    int bus_mhz;
    int clocks_valid;

    int mem_user_mb;
    int mem_cdram_mb;
    int mem_phy_mb;
    int mem_valid;

    StorageSample storage[STORAGE_COUNT]; /* 0 ux0, 1 uma0 */

    int batt_pct;
    int batt_temp_c;
    int batt_mv;
    int batt_cycles;
    int batt_health;
    int charging;
    int power_valid;

    PlatformModel model;
    char fw[FW_STRING_MAX];
    int fw_valid;

    double uptime_s;
    float fps;
    float frame_ms;

    int clock_h;
    int clock_m;
    int clock_s;
    int clock_valid;

    History hist_mem;   /* free user memory, MB */
    History hist_frame; /* frame time, ms */

    /*
     * The CORE trace's axis, computed once per slow sample rather than per
     * frame (D21 prices it at 10 Hz; the draw loop runs at 60).
     */
    float hist_mem_lo;
    float hist_mem_hi;

    /* Drives the hazard band. Never true on a healthy console. */
    int alert;
} Metrics;

extern Metrics g_metrics;

void metrics_init(void);

/* ~10 Hz. Clocks, memory, storage, power, identity, wall clock. */
void metrics_sample_slow(void);

/* Every frame. Frame time and FPS only. */
void metrics_sample_frame(float dt);

#endif
