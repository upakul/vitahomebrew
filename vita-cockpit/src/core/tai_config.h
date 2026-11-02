#ifndef COCKPIT_CORE_TAI_CONFIG_H
#define COCKPIT_CORE_TAI_CONFIG_H

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "core/plugins.h"

/*
 * The small taiHEN config grammar shared by the launch-set scanner
 * (core/plugins.c) and the editable line model (core/plugin_editor.c).
 *
 * Keeping these here is what stops the two readers from disagreeing about a
 * section name, a plugin suffix, or a path key. The line models themselves stay
 * separate: HOME needs only the activated set, while PLUGINS needs offsets,
 * line endings, and reachability.
 */

static inline int tai_ieq(const char *a, const char *b)
{
    while (*a != '\0' && *b != '\0') {
        char ca = *a;
        char cb = *b;

        if (ca >= 'A' && ca <= 'Z')
            ca = (char)(ca - 'A' + 'a');
        if (cb >= 'A' && cb <= 'Z')
            cb = (char)(cb - 'A' + 'a');

        if (ca != cb)
            return 0;

        ++a;
        ++b;
    }

    return *a == '\0' && *b == '\0';
}

static inline const char *tai_base_name(const char *path)
{
    const char *base = path;

    for (const char *p = path; *p != '\0'; ++p) {
        if (*p == '/' || *p == ':')
            base = p + 1;
    }

    return base;
}

static inline int tai_ends_with(const char *s, const char *suffix)
{
    size_t n = strlen(s);
    size_t m = strlen(suffix);

    return n >= m && strcmp(s + n - m, suffix) == 0;
}

static inline int tai_is_plugin_path(const char *s)
{
    return tai_ends_with(s, ".skprx") || tai_ends_with(s, ".suprx");
}

static inline int tai_is_plugin_path_slice(const char *s, int len)
{
    return (len >= 6 && memcmp(s + len - 6, ".skprx", 6) == 0) ||
           (len >= 6 && memcmp(s + len - 6, ".suprx", 6) == 0);
}

/*
 * Section names are compared exactly, because taiHEN uses strcmp; `*main` is
 * lowercase and `*ALL` is uppercase. Unknown targets are title IDs.
 */
static inline PluginSection tai_section_from_header(const char *line, int len)
{
    const char *name = line + 1;
    int n = len - 1;

    if (n == 6 && memcmp(name, "KERNEL", 6) == 0)
        return PLUGIN_SECTION_KERNEL;
    if (n == 4 && memcmp(name, "main", 4) == 0)
        return PLUGIN_SECTION_SYSTEM;
    if (n == 3 && memcmp(name, "ALL", 3) == 0)
        return PLUGIN_SECTION_ALL_TITLES;
    if (n >= 4 && memcmp(name, "NPXS", 4) == 0)
        return PLUGIN_SECTION_SYSTEM_APP;

    return PLUGIN_SECTION_TITLE;
}

static inline void tai_copy(char *dst, size_t cap, const char *src)
{
    if (cap == 0)
        return;

    snprintf(dst, cap, "%s", src);
}

static inline void tai_copy_slice(char *dst, size_t cap, const char *src,
                                  int len)
{
    if (cap == 0)
        return;

    size_t n = (size_t)len;
    if (n >= cap)
        n = cap - 1;

    memcpy(dst, src, n);
    dst[n] = '\0';
}

#endif
