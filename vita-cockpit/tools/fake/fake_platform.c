#include "fake_platform.h"

#include <stdio.h>
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

static void files_reset(void);

void fake_reset(void)
{
    files_reset();

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
 * A representative tai config and the files it names. The set exercises the
 * locked row (henkaku.suprx), a warned row (storagemgr.skprx), a disabled row,
 * an unknown description, an orphan, and a boot module.
 */
static const char SAMPLE_CONFIG[] =
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
    "ur0:tai/vita3g-disable.suprx\n";

/* Present so tests can prove the app never opens or copies it. */
static const char SAMPLE_BOOT[] =
    "- load\tur0:tai/taihen.skprx\n"
    "- load\tur0:tai/henkaku.skprx\n";

#define FAKE_FILES 64
#define FAKE_FILE_MAX 32768

typedef struct {
    char path[96];
    char data[FAKE_FILE_MAX];
    int len;
    int used;
} FakeFile;

/*
 * One failure record per operation. An empty path means "any file"; the
 * rename record also carries the stage mask so a test can fail the final move,
 * the restore, or both with one setter.
 */
typedef struct {
    int on;
    int stage;
    char path[PLATFORM_PATH_MAX];
} FakeFail;

static FakeFile s_files[FAKE_FILES];
static FakeFail s_fail_read;
static FakeFail s_fail_write;
static FakeFail s_fail_rename;
static int s_fail_backup;
static const char *s_write_error = "NONE";

static int fail_hit(const FakeFail *f, const char *path, int stage)
{
    if (!f->on || path == NULL)
        return 0;
    if (f->path[0] != '\0' && strcmp(f->path, path) != 0)
        return 0;
    if (stage != 0 && (f->stage & stage) == 0)
        return 0;

    return 1;
}

static void fail_set(FakeFail *f, const char *path, int stage)
{
    f->on = stage != 0;
    f->stage = stage;
    f->path[0] = '\0';

    if (path != NULL)
        snprintf(f->path, sizeof(f->path), "%s", path);
}

static FakeFile *find_file(const char *path)
{
    if (path == NULL)
        return NULL;

    for (int i = 0; i < FAKE_FILES; ++i) {
        if (s_files[i].used && strcmp(s_files[i].path, path) == 0)
            return &s_files[i];
    }

    return NULL;
}

static FakeFile *make_file(const char *path)
{
    if (path == NULL || strlen(path) >= sizeof(s_files[0].path))
        return NULL;

    for (int i = 0; i < FAKE_FILES; ++i) {
        if (!s_files[i].used) {
            s_files[i].used = 1;
            snprintf(s_files[i].path, sizeof(s_files[i].path), "%s", path);
            s_files[i].len = 0;
            s_files[i].data[0] = '\0';
            return &s_files[i];
        }
    }

    return NULL;
}

static void remove_file(const char *path)
{
    FakeFile *f = find_file(path);
    if (f != NULL)
        f->used = 0;
}

static void add_file(const char *path, const char *text)
{
    FakeFile *f = find_file(path);
    if (f == NULL)
        f = make_file(path);
    if (f == NULL)
        return;

    f->len = (int)strlen(text);
    if (f->len >= FAKE_FILE_MAX)
        f->len = FAKE_FILE_MAX - 1;
    memcpy(f->data, text, (size_t)f->len);
    f->data[f->len] = '\0';
}

static void files_reset(void)
{
    memset(s_files, 0, sizeof(s_files));
    memset(&s_fail_read, 0, sizeof(s_fail_read));
    memset(&s_fail_write, 0, sizeof(s_fail_write));
    memset(&s_fail_rename, 0, sizeof(s_fail_rename));
    s_fail_backup = 0;
    s_write_error = "NONE";

    add_file("ur0:tai/config.txt", SAMPLE_CONFIG);
    add_file("ur0:tai/boot_config.txt", SAMPLE_BOOT);

    add_file("ur0:tai/kubridge.skprx", "");
    add_file("ur0:tai/storagemgr.skprx", "");
    add_file("ur0:tai/0syscall6.skprx", "");
    add_file("ur0:tai/henkaku.suprx", "");
    add_file("ur0:tai/libshacccg.suprx", "");
    add_file("ur0:tai/shellbat.suprx", "");
    add_file("ur0:tai/mystery.suprx", "");
    add_file("ur0:tai/yamt.suprx", "");
    add_file("ur0:tai/vita3g-disable.suprx", "");
    add_file("ur0:tai/extra.skprx", "");
}

void fake_add_file(const char *path, const char *text)
{
    add_file(path, text == NULL ? "" : text);
}

void fake_remove_file(const char *path)
{
    remove_file(path);
}

void fake_fail_backup(int on)
{
    s_fail_backup = on;
}

void fake_fail_read(int on)
{
    fail_set(&s_fail_read, NULL, on ? 1 : 0);
}

void fake_fail_read_path(const char *path, int on)
{
    fail_set(&s_fail_read, path, on ? 1 : 0);
}

void fake_fail_write_path(const char *path, int on)
{
    fail_set(&s_fail_write, path, on ? 1 : 0);
}

void fake_fail_rename_path(const char *path, int stage)
{
    fail_set(&s_fail_rename, path, stage);
}

PlatformReadStatus platform_read_config(const char *path, char *buf, int cap,
                                        int *out_len)
{
    if (path == NULL || buf == NULL || cap <= 1)
        return PLATFORM_READ_UNREADABLE;

    FakeFile *f = find_file(path);
    if (f == NULL)
        return PLATFORM_READ_ABSENT;
    if (fail_hit(&s_fail_read, path, 0))
        return PLATFORM_READ_UNREADABLE;
    if (f->len >= cap)
        return PLATFORM_READ_TOO_LARGE;

    memcpy(buf, f->data, (size_t)f->len);
    buf[f->len] = '\0';
    if (out_len != NULL)
        *out_len = f->len;

    return PLATFORM_READ_OK;
}

static int verify_bytes(const char *path, const char *text, int len)
{
    FakeFile *f = find_file(path);

    if (f == NULL || f->len != len)
        return -1;
    if (len > 0 && memcmp(f->data, text, (size_t)len) != 0)
        return -1;

    return 0;
}

static int rename_file(const char *src, const char *dst)
{
    FakeFile *from = find_file(src);
    if (from == NULL)
        return -1;

    FakeFile *to = find_file(dst);
    if (to == NULL)
        to = make_file(dst);
    if (to == NULL)
        return -1;

    int len = from->len;
    if (len >= FAKE_FILE_MAX)
        return -1;

    memcpy(to->data, from->data, (size_t)len);
    to->data[len] = '\0';
    to->len = len;
    from->used = 0;
    return 0;
}

const char *platform_write_error(void)
{
    return s_write_error;
}

int platform_write_config(const char *path, const char *text, int len)
{
    s_write_error = "NONE";

    if (path == NULL || text == NULL || len < 0) {
        s_write_error = "ARGS";
        return -1;
    }

    if (fail_hit(&s_fail_write, path, 0)) {
        s_write_error = "DATA";
        return -1;
    }

    char tmp[128];
    char old[128];

    if (snprintf(tmp, sizeof(tmp), "%s.new", path) >= (int)sizeof(tmp) ||
        snprintf(old, sizeof(old), "%s.old", path) >= (int)sizeof(old)) {
        s_write_error = "PATH";
        return -1;
    }

    remove_file(tmp);

    remove_file(old);

    FakeFile *f = make_file(tmp);
    if (f == NULL || len >= FAKE_FILE_MAX) {
        s_write_error = "OPEN";
        return -1;
    }

    memcpy(f->data, text, (size_t)len);
    f->data[len] = '\0';
    f->len = len;

    if (verify_bytes(tmp, text, len) != 0) {
        remove_file(tmp);
        s_write_error = "VERIFY";
        return -1;
    }

    int had_original = find_file(path) != NULL;

    if (had_original && rename_file(path, old) != 0) {
        remove_file(tmp);
        s_write_error = "MOVE";
        return -1;
    }

    int final_failed = fail_hit(&s_fail_rename, path, FAKE_RENAME_FINAL);

    if (!final_failed)
        final_failed = rename_file(tmp, path) != 0;

    if (final_failed) {
        if (had_original) {
            int restore_failed =
                fail_hit(&s_fail_rename, path, FAKE_RENAME_RESTORE) ||
                rename_file(old, path) != 0;

            if (restore_failed) {
                remove_file(tmp);
                s_write_error = "RESTORE";
                return -1;
            }
        }

        remove_file(tmp);
        s_write_error = "RENAME";
        return -1;
    }

    if (verify_bytes(path, text, len) != 0) {
        if (had_original) {
            int restore_failed =
                fail_hit(&s_fail_rename, path, FAKE_RENAME_RESTORE) ||
                rename_file(old, path) != 0;

            if (restore_failed) {
                s_write_error = "RESTORE";
                return -1;
            }
        } else {
            remove_file(path);
        }

        s_write_error = "READBACK";
        return -1;
    }

    if (had_original)
        remove_file(old);

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

    if (path == NULL || text == NULL || len < 0 || find_file(path) == NULL)
        return -1;

    if (s_fail_backup)
        return -1;

    char dst[128];

    if (snprintf(dst, sizeof(dst), "%s.original", path) >= (int)sizeof(dst))
        have_original = 0;
    else if (find_file(dst) == NULL)
        have_original = platform_write_config(dst, text, len) == 0;

    if (snprintf(dst, sizeof(dst), "%s.session", path) >= (int)sizeof(dst))
        have_session = 0;
    else
        have_session = platform_write_config(dst, text, len) == 0;

    if (original_ok != NULL)
        *original_ok = have_original;
    if (session_ok != NULL)
        *session_ok = have_session;

    return (have_original && have_session) ? 0 : -1;
}

int platform_recover_swap(const char *path)
{
    char old[128];

    if (path == NULL)
        return 0;
    if (snprintf(old, sizeof(old), "%s.old", path) >= (int)sizeof(old))
        return 0;

    if (find_file(path) != NULL || find_file(old) == NULL)
        return 0;

    return rename_file(old, path) == 0 ? 1 : -1;
}

int platform_snapshot_undo(const char *path, const char *text, int len)
{
    char dst[128];

    if (path == NULL ||
        snprintf(dst, sizeof(dst), "%s.undo", path) >= (int)sizeof(dst))
        return -1;

    return platform_write_config(dst, text, len);
}

PlatformReadStatus platform_read_undo(const char *path, char *buf, int cap,
                                      int *out_len)
{
    char dst[128];

    if (path == NULL ||
        snprintf(dst, sizeof(dst), "%s.undo", path) >= (int)sizeof(dst))
        return PLATFORM_READ_UNREADABLE;

    return platform_read_config(dst, buf, cap, out_len);
}

int platform_remove_undo(const char *path)
{
    char dst[128];

    if (path == NULL ||
        snprintf(dst, sizeof(dst), "%s.undo", path) >= (int)sizeof(dst))
        return -1;

    remove_file(dst);
    return 0;
}

int platform_request_reboot(void)
{
    return 0;
}

int platform_list_dir(const char *dirname, PlatformDirCallback cb, void *user)
{
    if (dirname == NULL || cb == NULL)
        return -1;

    size_t prefix = strlen(dirname);
    int count = 0;

    for (int i = 0; i < FAKE_FILES; ++i) {
        if (!s_files[i].used)
            continue;
        if (strncmp(s_files[i].path, dirname, prefix) != 0)
            continue;
        if (s_files[i].path[prefix] != '/')
            continue;

        cb(s_files[i].path + prefix + 1, 0, user);
        ++count;
    }

    return count;
}

int platform_file_exists(const char *path)
{
    return find_file(path) != NULL;
}
