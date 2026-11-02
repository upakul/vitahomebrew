#ifndef COCKPIT_CORE_PLATFORM_H
#define COCKPIT_CORE_PLATFORM_H

#include <stdint.h>

/*
 * The port: what the machine can report, in plain C types.
 *
 * No Sce* type crosses this header (D7). The implementation lives in
 * platform/platform_psp2.c, which is where every sampled reading is taken. That
 * matters for three reasons:
 *
 *   1. The sampling policy in core/metrics.c can be compiled and exercised on
 *      a host machine against a fake implementation, with no Vita present.
 *   2. The one remaining [C] reading — the devctl storage unit — is corrected
 *      in one file, not in a panel.
 *   3. A panel that wants a new number is forced to add it here, so it cannot
 *      quietly reach for the SDK.
 *
 * Every reader sets `ok` explicitly. A zeroed struct means "nothing was read",
 * never "the reading was zero".
 */

#define PLATFORM_STORAGE_COUNT 2
#define PLATFORM_FW_MAX 32

/*
 * Mounts, in the order platform_read_storage() indexes them.
 *
 * ux0: is the memory card (or the SD2Vita card standing in for it) and uma0: is
 * the second removable volume. imc0: was dropped: it only exists on PCH-2000,
 * and on the 1000 it is always absent, so it spent a third of the panel saying
 * "--" on the console this targets (D33).
 */
#define PLATFORM_STORAGE_UX0 0
#define PLATFORM_STORAGE_UMA0 1

/*
 * Deliberately no PCH-1000 / PCH-2000 split. sceKernelGetModel() returns the
 * same code for both, so naming either one would be a guess (rule 6).
 */
typedef enum {
    PLATFORM_MODEL_UNKNOWN = 0,
    PLATFORM_MODEL_VITA,
    PLATFORM_MODEL_VITA_TV,
} PlatformModel;

typedef struct {
    uint64_t free_bytes;
    uint64_t total_bytes;
    /*
     * The platform call's own return code, 0 on success. Carried through so a
     * failed mount can say why instead of only "--": a denied devctl and a
     * malformed reply look identical otherwise, and telling them apart on
     * hardware is the difference between a permission problem and a layout one.
     */
    int error;
    /*
     * The low byte of `error`, decoded here because the layout of an SCE error
     * is a [C] platform claim (D7). A panel prints this; it never masks.
     */
    unsigned errno_byte;
    int ok;
} PlatformStorage;

typedef struct {
    int arm_mhz;
    int gpu_mhz;
    int bus_mhz;
    int ok;
} PlatformClocks;

typedef struct {
    uint64_t user_bytes;
    uint64_t cdram_bytes;
    uint64_t phycont_bytes;
    int ok;
} PlatformMemory;

typedef struct {
    int percent;
    int temp_c;
    int millivolts;
    int cycles;
    int health;   /* state of health, percent */
    int charging;
    int ok;
} PlatformPower;

typedef struct {
    PlatformModel model;
    char fw[PLATFORM_FW_MAX];
    int fw_ok;
} PlatformIdentity;

typedef struct {
    int hour;
    int minute;
    int second;
    int ok;
} PlatformClock;

void platform_read_storage(int index, PlatformStorage *out);
void platform_read_clocks(PlatformClocks *out);
void platform_read_memory(PlatformMemory *out);
void platform_read_power(PlatformPower *out);
void platform_read_identity(PlatformIdentity *out);
void platform_read_clock(PlatformClock *out);

/*
 * Text file access, for the plugin config.
 *
 * Reading a file is a device detail, so it crosses the port like any other
 * reading. `platform_read_text_file` NUL-terminates and returns the byte count,
 * or a negative value if the file is missing or unreadable; `platform_file_exists`
 * answers without reading, which is what "is this plugin actually loaded"
 * needs.
 */
int platform_read_text_file(const char *path, char *buf, int cap);
int platform_file_exists(const char *path);

/* Seconds since the calling process started. Not system uptime. */
double platform_uptime_seconds(void);

#endif
