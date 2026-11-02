#ifndef COCKPIT_TOOLS_FAKE_PLATFORM_H
#define COCKPIT_TOOLS_FAKE_PLATFORM_H

#include <stdint.h>

/*
 * A controllable implementation of core/platform.h, shared by tools/preview and
 * tools/test.
 *
 * This is the test double the port exists for (D7). Scenario control lives
 * here, so the preview drives the real sampler through the real policy instead
 * of writing g_metrics by hand -- which is what made the old preview prove
 * nothing about validity propagation (D26).
 */

/* Healthy console: ux0 present, uma0 absent, 182 MB free user. */
void fake_reset(void);

void fake_set_memory_mb(int user_mb);
void fake_set_memory_absent(void);

void fake_set_power(int percent, int charging);
void fake_set_power_absent(void);

void fake_set_storage(int index, uint64_t total_bytes, uint64_t free_bytes);
void fake_set_storage_absent(int index);

void fake_set_identity_absent(void);
void fake_set_clock_absent(void);

void fake_add_file(const char *path, const char *text);
void fake_remove_file(const char *path);
void fake_fail_backup(int on);
void fake_fail_read(int on);
void fake_fail_read_path(const char *path, int on);
void fake_fail_write_path(const char *path, int on);

typedef enum {
    FAKE_RENAME_NONE = 0,
    FAKE_RENAME_FINAL = 1,
    FAKE_RENAME_RESTORE = 2,
} FakeRenameStage;

void fake_fail_rename_path(const char *path, int stage);

#endif
