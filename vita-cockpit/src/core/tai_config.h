#ifndef COCKPIT_CORE_TAI_CONFIG_H
#define COCKPIT_CORE_TAI_CONFIG_H

#include <stddef.h>
#include <string.h>

#include "core/platform.h"
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

#define TAI_CONFIG_UX0 "ux0:tai/config.txt"
#define TAI_CONFIG_UR0 "ur0:tai/config.txt"
#define TAI_CONFIG_MAX 32768

#define TAI_DIR_UX0 "ux0:tai"
#define TAI_DIR_UR0 "ur0:tai"

/*
 * taiHEN opens ux0: first and falls back to ur0: only when ux0: is absent.
 * An unreadable ux0: is still the active file; it must not be shadowed by a
 * writable ur0:. Both scanners use this one decision (D71).
 */
typedef struct {
    const char *path; /* NULL when neither mount has a file */
    PlatformReadStatus status;
} TaiConfigChoice;

static inline TaiConfigChoice tai_config_choose(PlatformReadStatus ux0,
                                                PlatformReadStatus ur0)
{
    TaiConfigChoice choice = {NULL, PLATFORM_READ_ABSENT};

    if (ux0 != PLATFORM_READ_ABSENT) {
        choice.path = TAI_CONFIG_UX0;
        choice.status = ux0;
    } else if (ur0 != PLATFORM_READ_ABSENT) {
        choice.path = TAI_CONFIG_UR0;
        choice.status = ur0;
    }

    return choice;
}

static inline char tai_lower(char c)
{
    if (c >= 'A' && c <= 'Z')
        return (char)(c - 'A' + 'a');

    return c;
}

static inline int tai_ieq(const char *a, const char *b)
{
    while (*a != '\0' && *b != '\0') {
        if (tai_lower(*a) != tai_lower(*b))
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

static inline int tai_slice_ends_with_ci(const char *s, int len,
                                         const char *suffix)
{
    int m = (int)strlen(suffix);

    if (len < m)
        return 0;

    for (int i = 0; i < m; ++i) {
        if (tai_lower(s[len - m + i]) != tai_lower(suffix[i]))
            return 0;
    }

    return 1;
}

static inline int tai_ends_with(const char *s, const char *suffix)
{
    return tai_slice_ends_with_ci(s, (int)strlen(s), suffix);
}

static inline int tai_is_plugin_path_slice(const char *s, int len)
{
    return tai_slice_ends_with_ci(s, len, ".skprx") ||
           tai_slice_ends_with_ci(s, len, ".suprx");
}

static inline int tai_is_plugin_path(const char *s)
{
    return tai_is_plugin_path_slice(s, (int)strlen(s));
}

/*
 * A config path has no whitespace and no control bytes. Without this check a
 * prose comment that happens to end in `.skprx` becomes a phantom row. The
 * rule is deliberately conservative: taiHEN reads the rest of the line as one
 * path, so a line with a space in it is ambiguous rather than a valid plugin.
 */
static inline int tai_path_valid(const char *s, int len)
{
    if (len <= 0)
        return 0;

    for (int i = 0; i < len; ++i) {
        unsigned char c = (unsigned char)s[i];

        if (c <= ' ' || c == 0x7f || c == '#')
            return 0;
    }

    return tai_is_plugin_path_slice(s, len);
}

#define TAI_BOM_LEN 3

static inline int tai_has_bom(const char *s, int len)
{
    return len >= TAI_BOM_LEN && (unsigned char)s[0] == 0xEF &&
           (unsigned char)s[1] == 0xBB && (unsigned char)s[2] == 0xBF;
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

    size_t n = strlen(src);
    if (n >= cap)
        n = cap - 1;

    memcpy(dst, src, n);
    dst[n] = '\0';
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
