#include "core/platform.h"

#include <string.h>

#include <psp2/appmgr.h>
#include <psp2/io/devctl.h>
#include <psp2/io/dirent.h>
#include <psp2/io/fcntl.h>
#include <psp2/io/stat.h>
#include <psp2/kernel/modulemgr.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/sysmem.h>
#include <psp2/power.h>
#include <psp2/rtc.h>
#include <stdio.h>

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

int platform_file_exists(const char *path)
{
    if (path == NULL)
        return 0;

    SceIoStat st;
    memset(&st, 0, sizeof(st));

    if (sceIoGetstat(path, &st) < 0) {
        /*
         * A stat-only failure must not turn an existing plugin into MISSING.
         * If the file can be opened, it exists; the row's status is about the
         * file, not about the metadata call.
         */
        SceUID fd = sceIoOpen(path, SCE_O_RDONLY, 0);
        if (fd < 0)
            return 0;

        (void)sceIoClose(fd);
        return 1;
    }

    return SCE_SO_ISDIR(st.st_mode) ? 0 : 1;
}

PlatformReadStatus platform_read_config(const char *path, char *buf, int cap,
                                        int *out_len)
{
    if (path == NULL || buf == NULL || cap <= 1)
        return PLATFORM_READ_UNREADABLE;

    /*
     * Open first. The previous version required sceIoGetstat to succeed before
     * it would open, so a stat-only failure looked like a missing config. The
     * file is what matters; the stat is only used to classify an open failure
     * and to catch a directory or a short read.
     */
    SceUID fd = sceIoOpen(path, SCE_O_RDONLY, 0);
    if (fd < 0) {
        SceIoStat st;
        memset(&st, 0, sizeof(st));

        if (sceIoGetstat(path, &st) < 0)
            return PLATFORM_READ_ABSENT;

        return PLATFORM_READ_UNREADABLE;
    }

    SceIoStat st;
    memset(&st, 0, sizeof(st));
    int have_stat = sceIoGetstat(path, &st) >= 0;

    if (have_stat && SCE_SO_ISDIR(st.st_mode)) {
        (void)sceIoClose(fd);
        return PLATFORM_READ_UNREADABLE;
    }

    int total = 0;

    for (;;) {
        if (total >= cap) {
            (void)sceIoClose(fd);
            return PLATFORM_READ_TOO_LARGE;
        }

        int n = sceIoRead(fd, buf + total, (SceSize)(cap - total));
        if (n < 0) {
            (void)sceIoClose(fd);
            return PLATFORM_READ_UNREADABLE;
        }
        if (n == 0)
            break;

        total += n;
    }

    (void)sceIoClose(fd);

    /*
     * A short read is not a smaller config: the parser would edit the prefix
     * and the rewrite would delete the tail. The stat size is the only external
     * check available; when it says more bytes were expected, refuse the read.
     * A zero or smaller size is treated as unknown, not as proof.
     */
    if (have_stat && st.st_size > 0 && (SceOff)total < st.st_size)
        return PLATFORM_READ_UNREADABLE;

    buf[total] = '\0';
    if (out_len != NULL)
        *out_len = total;

    return PLATFORM_READ_OK;
}

static int verify_bytes(const char *path, const char *text, int len)
{
    static char check[PLATFORM_CONFIG_MAX];
    int checked = 0;

    if (platform_read_config(path, check, (int)sizeof(check), &checked) !=
        PLATFORM_READ_OK)
        return -1;
    if (checked != len || memcmp(check, text, (size_t)len) != 0)
        return -1;

    return 0;
}

static int sibling_path(char *dst, size_t cap, const char *path,
                        const char *suffix);

static const char *s_write_error = "NONE";

const char *platform_write_error(void)
{
    return s_write_error;
}

int platform_write_config(const char *path, const char *text, int len)
{
    s_write_error = "NONE";

    if (path == NULL || text == NULL || len < 0 ||
        len >= PLATFORM_CONFIG_MAX) {
        s_write_error = "ARGS";
        return -1;
    }

    char tmp[PLATFORM_PATH_MAX];
    char old[PLATFORM_PATH_MAX];

    if (sibling_path(tmp, sizeof(tmp), path, ".new") != 0 ||
        sibling_path(old, sizeof(old), path, ".old") != 0) {
        s_write_error = "PATH";
        return -1;
    }

    (void)sceIoRemove(tmp);

    /*
     * A stale .old means an earlier swap was interrupted. It is the previous
     * config, so it must not be overwritten silently; remove it first and fail
     * closed if it cannot be removed.
     */
    if (sceIoRemove(old) < 0 && platform_file_exists(old)) {
        s_write_error = "OLD";
        return -1;
    }

    SceUID fd = sceIoOpen(tmp, SCE_O_WRONLY | SCE_O_CREAT | SCE_O_EXCL, 0666);
    if (fd < 0) {
        s_write_error = "OPEN";
        return -1;
    }

    int done = 0;
    while (done < len) {
        int w = sceIoWrite(fd, text + done, (SceSize)(len - done));
        if (w <= 0) {
            (void)sceIoClose(fd);
            (void)sceIoRemove(tmp);
            s_write_error = "DATA";
            return -1;
        }
        done += w;
    }

    /*
     * Best-effort flush. The read-back below is the correctness check; a
     * filesystem that refuses the sync must not turn every edit into a DATA
     * failure. The swap still leaves .old recoverable if power is lost.
     */
    (void)sceIoSyncByFd(fd, 0);

    if (sceIoClose(fd) < 0) {
        (void)sceIoRemove(tmp);
        s_write_error = "CLOSE";
        return -1;
    }

    if (verify_bytes(tmp, text, len) != 0) {
        (void)sceIoRemove(tmp);
        s_write_error = "VERIFY";
        return -1;
    }

    int had_original = platform_file_exists(path);

    /*
     * Rename-away instead of rename-over. The old file is moved to an absent
     * name, the verified sibling takes its place, and the live file is read
     * back. This does not depend on the filesystem replacing an existing target.
     */
    if (had_original && sceIoRename(path, old) < 0) {
        (void)sceIoRemove(tmp);
        s_write_error = "MOVE";
        return -1;
    }

    if (sceIoRename(tmp, path) < 0) {
        if (had_original && sceIoRename(old, path) < 0) {
            (void)sceIoRemove(tmp);
            s_write_error = "RESTORE";
            return -1;
        }

        (void)sceIoRemove(tmp);
        s_write_error = "RENAME";
        return -1;
    }

    if (verify_bytes(path, text, len) != 0) {
        /*
         * The replacement was observed to be wrong. Put the original back if
         * possible; if the rename is refused, leave both files for manual
         * recovery rather than deleting the only remaining config.
         */
        if (had_original && sceIoRename(old, path) < 0) {
            s_write_error = "RESTORE";
            return -1;
        }

        if (!had_original)
            (void)sceIoRemove(path);

        s_write_error = "READBACK";
        return -1;
    }

    if (had_original)
        (void)sceIoRemove(old);

    return 0;
}

int platform_backup_config(const char *path, const char *text, int len,
                           int *original_ok, int *session_ok)
{
    int have_original = 1;
    int have_session = 1;

    if (original_ok != NULL)
        *original_ok = 0;
    if (session_ok != NULL)
        *session_ok = 0;

    if (path == NULL || text == NULL || len < 0)
        return -1;

    /*
     * The caller only calls this after a successful read, so a missing source
     * means the file disappeared between read and snapshot. Treat both
     * snapshots as unavailable rather than writing stale bytes.
     */
    if (!platform_file_exists(path))
        return -1;

    char dst[PLATFORM_PATH_MAX];

    if (sibling_path(dst, sizeof(dst), path, ".original") != 0)
        have_original = 0;
    else if (!platform_file_exists(dst))
        have_original = platform_write_config(dst, text, len) == 0;

    if (sibling_path(dst, sizeof(dst), path, ".session") != 0)
        have_session = 0;
    else
        have_session = platform_write_config(dst, text, len) == 0;

    if (original_ok != NULL)
        *original_ok = have_original;
    if (session_ok != NULL)
        *session_ok = have_session;

    return (have_original && have_session) ? 0 : -1;
}

static int sibling_path(char *dst, size_t cap, const char *path,
                        const char *suffix)
{
    int n;

    if (path == NULL || suffix == NULL || cap == 0)
        return -1;

    n = snprintf(dst, cap, "%s%s", path, suffix);
    return (n > 0 && (size_t)n < cap) ? 0 : -1;
}

int platform_recover_swap(const char *path)
{
    char old[PLATFORM_PATH_MAX];

    if (path == NULL)
        return 0;
    if (sibling_path(old, sizeof(old), path, ".old") != 0)
        return 0;

    if (platform_file_exists(path) || !platform_file_exists(old))
        return 0;

    return sceIoRename(old, path) < 0 ? -1 : 1;
}

int platform_snapshot_undo(const char *path, const char *text, int len)
{
    char dst[PLATFORM_PATH_MAX];

    if (sibling_path(dst, sizeof(dst), path, ".undo") != 0)
        return -1;

    return platform_write_config(dst, text, len);
}

PlatformReadStatus platform_read_undo(const char *path, char *buf, int cap,
                                      int *out_len)
{
    char dst[PLATFORM_PATH_MAX];

    if (sibling_path(dst, sizeof(dst), path, ".undo") != 0)
        return PLATFORM_READ_UNREADABLE;

    return platform_read_config(dst, buf, cap, out_len);
}

int platform_remove_undo(const char *path)
{
    char dst[PLATFORM_PATH_MAX];

    if (sibling_path(dst, sizeof(dst), path, ".undo") != 0)
        return -1;
    if (sceIoRemove(dst) < 0 && platform_file_exists(dst))
        return -1;

    return 0;
}

int platform_request_reboot(void)
{
    /* [V] psp2/power.h: int scePowerRequestColdReset(void). [C] that a userland
     * UNSAFE eboot on 3.65 Enso is allowed to perform the reset. */
    return scePowerRequestColdReset();
}

int platform_list_dir(const char *dirname, PlatformDirCallback cb, void *user)
{
    if (dirname == NULL || cb == NULL)
        return -1;

    SceUID dfd = sceIoDopen(dirname);
    if (dfd < 0) {
        /*
         * A tai directory that does not exist is normal (most consoles have
         * only one of ur0:tai and ux0:tai). An existing directory that cannot
         * be opened is a real error and must stay visible.
         */
        SceIoStat st;
        memset(&st, 0, sizeof(st));

        if (sceIoGetstat(dirname, &st) >= 0 && SCE_SO_ISDIR(st.st_mode))
            return -1;

        return 0;
    }

    int count = 0;
    SceIoDirent entry;

    for (;;) {
        memset(&entry, 0, sizeof(entry));
        int n = sceIoDread(dfd, &entry);
        if (n < 0) {
            (void)sceIoDclose(dfd);
            return -1;
        }
        if (n == 0)
            break;

        if (strcmp(entry.d_name, ".") == 0 || strcmp(entry.d_name, "..") == 0)
            continue;

        cb(entry.d_name, SCE_SO_ISDIR(entry.d_stat.st_mode) ? 1 : 0, user);
        ++count;
    }

    (void)sceIoDclose(dfd);
    return count;
}
