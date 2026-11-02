#include "core/platform.h"

#include <string.h>

#include <psp2/io/devctl.h>
#include <psp2/kernel/modulemgr.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/sysmem.h>
#include <psp2/power.h>
#include <psp2/rtc.h>

/*
 * The PSP2 SDK adapter. This is the only file in COCKPIT that may include
 * psp2 headers alongside the sampling policy's port (D7, D8).
 *
 * Every [C] claim in the project lives here, so that correcting one after a
 * hardware session is a one-file change that cannot reach into core/metrics.c
 * or any panel. See docs/ARCHITECTURE.md §7.
 */

static const char *const MOUNTS[PLATFORM_STORAGE_COUNT] = {
    "ux0:",
    "imc0:",
    "uma0:",
};

/*
 * [C] Command 0x3001 is the community-documented "get device info" devctl.
 * [C] max_size and free_size are bytes, not clusters. The headers declare
 * SceIoDevInfo (psp2common/kernel/iofilemgr.h, 24 bytes) but say nothing about
 * the unit, so a wrong unit would show a 29 GB card as 29 MB.
 */
#define DEVCTL_GET_DEVICE_INFO 0x3001u

/* A mount smaller than this is a misread struct, not a small card. */
#define MIN_PLAUSIBLE_BYTES (1024u * 1024u)

/*
 * [C] The model codes are absent from every installed header, though
 * psp2/kernel/sysmem.h says the return is "a value from SCE_KERNEL_MODEL" and
 * never defines that enum. 0x10000 is the Vita family, 0x20000 the PSTV.
 *
 * There is no code for PCH-1000 versus PCH-2000: the API returns 0x10000 for
 * both. COCKPIT therefore reports "PSVITA" rather than inventing a variant.
 */
#define MODEL_CODE_VITA 0x10000
#define MODEL_CODE_VITA_TV 0x20000

void platform_read_storage(int index, PlatformStorage *out)
{
    if (out == NULL)
        return;

    memset(out, 0, sizeof(*out));

    if (index < 0 || index >= PLATFORM_STORAGE_COUNT)
        return;

    /* [V] SceIoDevInfo is declared in psp2common/kernel/iofilemgr.h at 24
     * bytes: two SceOff (64-bit), one SceSize, one pointer. */
    SceIoDevInfo info;
    memset(&info, 0, sizeof(info));

    int rc = sceIoDevctl(MOUNTS[index], DEVCTL_GET_DEVICE_INFO, NULL, 0, &info,
                         sizeof(info));

    /* A card that is not inserted fails the devctl; that is not an error. */
    if (rc < 0 || info.max_size <= 0)
        return;

    /*
     * Sanity bounds, not guesses. If the [C] struct layout or unit is wrong,
     * these turn nonsense into "--" instead of printing a fabricated capacity.
     * A real mount is at least a megabyte and never reports more free than it
     * has.
     */
    if (info.max_size < (SceOff)MIN_PLAUSIBLE_BYTES)
        return;
    if (info.free_size < 0 || info.free_size > info.max_size)
        return;

    out->total_bytes = (uint64_t)info.max_size;
    out->free_bytes = (uint64_t)info.free_size;
    out->ok = 1;
}

void platform_read_clocks(PlatformClocks *out)
{
    if (out == NULL)
        return;

    memset(out, 0, sizeof(*out));

    /*
     * [V] psp2/power.h: these return the current frequency in MHz, not a
     * capability ceiling. A value <= 0 is a failed read, not a clock of 0.
     */
    int arm = scePowerGetArmClockFrequency();
    int gpu = scePowerGetGpuClockFrequency();
    int bus = scePowerGetBusClockFrequency();

    if (arm <= 0 || gpu <= 0 || bus <= 0)
        return;

    out->arm_mhz = arm;
    out->gpu_mhz = gpu;
    out->bus_mhz = bus;
    out->ok = 1;
}

void platform_read_memory(PlatformMemory *out)
{
    if (out == NULL)
        return;

    memset(out, 0, sizeof(*out));

    /* [V] psp2/kernel/sysmem.h: sizes are bytes, `size` must be set first. */
    SceKernelFreeMemorySizeInfo info;
    memset(&info, 0, sizeof(info));
    info.size = sizeof(info);

    if (sceKernelGetFreeMemorySize(&info) < 0)
        return;

    /* The struct's fields are signed ints; a negative is a bad read. */
    if (info.size_user < 0 || info.size_cdram < 0 || info.size_phycont < 0)
        return;

    out->user_bytes = (uint64_t)(unsigned)info.size_user;
    out->cdram_bytes = (uint64_t)(unsigned)info.size_cdram;
    out->phycont_bytes = (uint64_t)(unsigned)info.size_phycont;
    out->ok = 1;
}

void platform_read_power(PlatformPower *out)
{
    if (out == NULL)
        return;

    memset(out, 0, sizeof(*out));

    /* [V] psp2/power.h: percent is the only reading with a documented failure
     * value. The rest are accepted as reported once percent is sane. */
    int pct = scePowerGetBatteryLifePercent();
    if (pct < 0)
        return;

    out->percent = pct;
    out->temp_c = scePowerGetBatteryTemp();
    out->millivolts = scePowerGetBatteryVolt();
    out->cycles = scePowerGetBatteryCycleCount();
    out->health = scePowerGetBatterySOH();
    out->charging = scePowerIsBatteryCharging() ? 1 : 0;
    out->ok = 1;
}

void platform_read_identity(PlatformIdentity *out)
{
    if (out == NULL)
        return;

    memset(out, 0, sizeof(*out));

    int model = sceKernelGetModel();
    if (model == MODEL_CODE_VITA)
        out->model = PLATFORM_MODEL_VITA;
    else if (model == MODEL_CODE_VITA_TV)
        out->model = PLATFORM_MODEL_VITA_TV;
    else
        out->model = PLATFORM_MODEL_UNKNOWN;

    /*
     * [V] psp2common/kernel/modulemgr.h declares SceKernelSystemSwVersion and
     * requires `size` to be set before the call. versionString is 0x1C bytes
     * and is not guaranteed to be terminated.
     */
    SceKernelSystemSwVersion v;
    memset(&v, 0, sizeof(v));
    v.size = sizeof(v);

    if (sceKernelGetSystemSwVersion(&v) == 0) {
        memcpy(out->fw, v.versionString, PLATFORM_FW_MAX - 1);
        out->fw[PLATFORM_FW_MAX - 1] = '\0';
        out->fw_ok = 1;
    }
}

void platform_read_clock(PlatformClock *out)
{
    if (out == NULL)
        return;

    memset(out, 0, sizeof(*out));

    /* [V] psp2/rtc.h: local time, already offset by the system time zone. */
    SceDateTime dt;
    memset(&dt, 0, sizeof(dt));

    if (sceRtcGetCurrentClockLocalTime(&dt) < 0)
        return;

    out->hour = dt.hour;
    out->minute = dt.minute;
    out->second = dt.second;
    out->ok = 1;
}

double platform_uptime_seconds(void)
{
    /* [V] psp2/kernel/processmgr.h: microseconds since this process started. */
    return (double)sceKernelGetProcessTimeWide() / 1000000.0;
}
