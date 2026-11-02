#include "core/platform.h"

#include <string.h>

#include <psp2/appmgr.h>
#include <psp2/io/devctl.h>
#include <psp2/io/fcntl.h>
#include <psp2/io/stat.h>
#include <psp2/kernel/modulemgr.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/sysmem.h>
#include <psp2/power.h>
#include <psp2/rtc.h>

/*
 * The PSP2 SDK adapter. This is the only file in COCKPIT that may include
 * psp2 headers alongside the sampling policy's port (D7, D8).
 *
 * Every [C] platform claim in the project lives here, so that correcting one
 * after a hardware session is a one-file change that cannot reach into
 * core/metrics.c or any panel. See docs/ARCHITECTURE.md §7.
 */

static const char *const MOUNTS[PLATFORM_STORAGE_COUNT] = {
    "ux0:",
    "uma0:",
};

/*
 * [C] Command 0x3001 is the community-documented "get device info" devctl, and
 * psp2/io/devctl.h demonstrates the call verbatim. What the reply *means* --
 * its unit in particular -- is documented nowhere, which is the one claim this
 * project still cannot make from a header.
 */
#define DEVCTL_GET_DEVICE_INFO 0x3001u

/*
 * [C] SCE error values carry the errno in the low byte: 0x80010000 | errno.
 * ENODEV (19) is what a devctl returns for a volume that is not mounted -- the
 * answer observed on hardware for uma0:. That is an absent row, not a fault.
 */
#define IO_ERROR_BASE 0x80010000u
#define IO_ENODEV ((int)(IO_ERROR_BASE | 19u))

/* [V] psp2/power.h: scePowerGetBatteryTemp returns "degrees celcius * 100". */
#define BATTERY_TEMP_SCALE 100

/*
 * [V] psp2common/kernel/sysmem.h defines SCE_KERNEL_MODEL_VITA (0x10000) and
 * SCE_KERNEL_MODEL_VITATV (0x20000); the enum arrives through the
 * psp2/kernel/sysmem.h include above.
 *
 * There is no code for PCH-1000 versus PCH-2000: the API returns 0x10000 for
 * both. COCKPIT therefore reports "PSVITA" rather than inventing a variant.
 */

void platform_read_storage(int index, PlatformStorage *out)
{
    if (out == NULL)
        return;

    memset(out, 0, sizeof(*out));

    if (index < 0 || index >= PLATFORM_STORAGE_COUNT)
        return;

    /*
     * [V] psp2/appmgr.h: sceAppMgrGetDevInfo reports capacity and free space in
     * bytes for a device.
     *
     * This is the primary route because it is an ordinary app-manager call, so
     * it works under a safe eboot. sceIoDevctl on ux0: does not: on hardware the
     * safe build returned 0x80010030 for a mounted card, and only the UNSAFE
     * build got a size (D38). Reading storage must not cost the whole system's
     * permissions.
     */
    uint64_t max_size = 0;
    uint64_t free_size = 0;
    int rc = sceAppMgrGetDevInfo(MOUNTS[index], &max_size, &free_size);

    if (rc == 0 && max_size > 0) {
        if (free_size > max_size)
            free_size = max_size;

        out->total_bytes = max_size;
        out->free_bytes = free_size;
        out->ok = 1;
        return;
    }

    /*
     * Fallback for a device AppMgr does not cover. The devctl answers ENODEV for
     * a volume that is not mounted, which is an absent row rather than a fault.
     */
    SceIoDevInfo info;
    memset(&info, 0, sizeof(info));

    int devctl_rc = sceIoDevctl(MOUNTS[index], DEVCTL_GET_DEVICE_INFO, NULL, 0,
                                &info, sizeof(info));

    if (devctl_rc == IO_ENODEV)
        return;

    out->error = devctl_rc != 0 ? devctl_rc : rc;
    out->errno_byte = (unsigned)(out->error & 0xFF);

    if (devctl_rc < 0 || info.max_size <= 0)
        return;

    uint64_t total_bytes = (uint64_t)info.max_size;
    uint64_t free_bytes = (uint64_t)info.free_size;

    /* Never underflow used = total - free, whatever the reply contains. */
    if (free_bytes > total_bytes)
        free_bytes = total_bytes;

    out->total_bytes = total_bytes;
    out->free_bytes = free_bytes;
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
    out->temp_c = scePowerGetBatteryTemp() / BATTERY_TEMP_SCALE;
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
    if (model == SCE_KERNEL_MODEL_VITA)
        out->model = PLATFORM_MODEL_VITA;
    else if (model == SCE_KERNEL_MODEL_VITATV)
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

int platform_read_text_file(const char *path, char *buf, int cap)
{
    if (path == NULL || buf == NULL || cap <= 1)
        return -1;

    SceUID fd = sceIoOpen(path, SCE_O_RDONLY, 0);
    if (fd < 0)
        return -1;

    int n = sceIoRead(fd, buf, (SceSize)(cap - 1));
    (void)sceIoClose(fd);

    if (n < 0)
        return -1;

    buf[n] = '\0';
    return n;
}

int platform_file_exists(const char *path)
{
    if (path == NULL)
        return 0;

    SceIoStat st;
    memset(&st, 0, sizeof(st));

    return sceIoGetstat(path, &st) >= 0;
}
