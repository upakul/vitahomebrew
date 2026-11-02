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

/*
 * The list is a fixed array, so it needs a ceiling. It has to clear a real
 * config: a heavy 3.65 setup boots around twenty plugins, and the old value of
 * 12 silently dropped the tail of `config.txt`. 64 leaves room for a console
 * that runs far more than that, and `truncated` makes the loss visible if the
 * ceiling is ever reached instead of hiding it (D50).
 */
#define PLUGINS_MAX 64

/* Longest catalog file name is 41 characters. 32 cut it and its description. */
#define PLUGIN_NAME_MAX 48

/* Room for a mount, a directory and a long file name. A truncated path would
 * fail the existence check and drop the row without saying why. */
#define PLUGIN_PATH_MAX 96

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
    int truncated;    /* the config listed more than PLUGINS_MAX plugins */
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
