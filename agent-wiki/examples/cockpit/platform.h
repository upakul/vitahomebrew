#ifndef COCKPIT_CORE_PLATFORM_H
#define COCKPIT_CORE_PLATFORM_H

#include <stdint.h>

/*
 * The port: what the machine can report, in plain C types.
 *
 * No Sce* type crosses this header (D7). The implementation lives in
 * platform/platform_psp2.c and is the only file that knows the SDK. That
 * matters for three reasons:
 *
 *   1. The sampling policy in core/metrics.c can be compiled and exercised on
 *      a host machine against a fake implementation, with no Vita present.
 *   2. Three readings rest on [C] community knowledge — SceIoDevInfo, the
 *      model codes, and the storage unit — and all three are corrected in one
 *      file.
 *   3. A panel that wants a new number is forced to add it here, so it cannot
 *      quietly reach for the SDK.
 *
 * Every reader sets `ok` explicitly. A zeroed struct means "nothing was read",
 * never "the reading was zero".
 */

#define PLATFORM_STORAGE_COUNT 3
#define PLATFORM_FW_MAX 32

/* Mounts, in the order platform_read_storage() indexes them. */
#define PLATFORM_STORAGE_UX0 0
#define PLATFORM_STORAGE_IMC0 1
#define PLATFORM_STORAGE_UMA0 2

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

/* Seconds since the calling process started. Not system uptime. */
double platform_uptime_seconds(void);

#endif
