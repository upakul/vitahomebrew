#ifndef COCKPIT_CORE_PLUGINS_H
#define COCKPIT_CORE_PLUGINS_H

#include <stdint.h>

/*
 * Which taiHEN plugins are loaded right now.
 *
 * Enso reads its plugin list from `ur0:tai/config.txt` (and the legacy
 * `ux0:tai/config.txt`), and loads every path listed under a section. A line
 * whose file is missing is skipped, so "listed" and "loaded" are not the same
 * thing: this module keeps only the entries whose file is actually on disk.
 *
 * Reading the files crosses the port; parsing does not, so the parser is
 * exercised on the host with sample config text.
 */

#define PLUGINS_MAX 12
#define PLUGIN_NAME_MAX 32
#define PLUGIN_PATH_MAX 64
#define PLUGIN_DESC_MAX 56

typedef struct {
    char name[PLUGIN_NAME_MAX]; /* basename, e.g. "kubridge.skprx" */
    char path[PLUGIN_PATH_MAX]; /* as written in config.txt */
    char desc[PLUGIN_DESC_MAX]; /* short role, or "" when unknown */
} PluginInfo;

typedef struct {
    PluginInfo items[PLUGINS_MAX];
    int count;
    int config_found; /* at least one config.txt was readable */
} PluginList;

extern PluginList g_plugins;

/*
 * Append every plugin listed in `text` to `list`. Pure: no file access, so it
 * is host-testable. Duplicates (the same basename from both configs) are
 * merged.
 */
int plugins_parse(const char *text, PluginList *list);

/* Read both configs through the port, then drop entries whose file is missing. */
void plugins_scan(void);

#endif
