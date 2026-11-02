#include "core/plugins.h"

#include <stdio.h>
#include <string.h>

#include "core/platform.h"
#include "core/tai_config.h"

PluginList g_plugins;

/*
 * The two places taiHEN looks for a plugin list. taiHEN opens ux0: first and
 * falls back to ur0:, so the app must use the same order (D55).
 */
static const char *const CONFIGS[] = {
    "ux0:tai/config.txt",
    "ur0:tai/config.txt",
};

#define CONFIG_COUNT ((int)(sizeof(CONFIGS) / sizeof(CONFIGS[0])))

/* Generous for a config file; the format is a few dozen short lines. */
#define CONFIG_CAP 32768

#define PLUGIN_SUFFIX_KERNEL ".skprx"
#define PLUGIN_SUFFIX_USER ".suprx"

/*
 * [C] Short descriptions of plugins common on 3.65 Enso. Community knowledge,
 * not something a header or the console reports; the canonical list is the
 * AutoPlugin2 project. An unknown plugin still appears — the name alone tells a
 * reader something — with an empty description rather than an invented one.
 */
typedef struct {
    const char *name;
    const char *desc;
} KnownPlugin;

static const KnownPlugin KNOWN[] = {
    /*
     * Every plugin in the AutoPlugin2 Vita catalog
     * (AUTOPLUG2/plugins/plugins.lua), keyed by file name and paired with its
     * English string (AUTOPLUG2/lang/ENGLISH_US.lua). The catalog's text is
     * shortened to one line; PLUGIN_DESC_MAX is the hard limit. Sorted by file
     * name so a new entry has an obvious place.
     *
     * A row marked [C] is not a straight catalog copy: the catalog may have no
     * entry for that file (iTLS, libshacccg, YAMT, HENkaku), list it only as a
     * dependency (ioPlus, kuio), or carry text that belongs to another plugin
     * or is unusable (adrenaline_kernel, ds3, lowmemmode, scorehax).
     */
    {"0syscall6.skprx", "patches lv0 syscall 6 checks"},
    {"8bitvita.skprx", "8bitdo controller support"},
    {"aciddumper.skprx", "locates fake dlc content"},
    /* [C] catalog text is the bubble launcher's, not the kernel module */
    {"adrenaline_kernel.skprx", "adrenaline kernel module"},
    {"adrenaline_usb_enabler.suprx", "adrenaline usb pairing"},
    {"alienhook.suprx", "alien shooter dlc unlocker"},
    {"allefresher_vita.suprx", "littlebigplanet custom server patch"},
    {"analogsenhancer.skprx", "analog deadzone and rescaling"},
    {"analogstickdisable.skprx", "disables both analog sticks"},
    {"analogstickdisable_leftonly.skprx", "disables left analog stick"},
    {"analogstickdisable_lefttoright.skprx", "left stick acts as right stick"},
    {"analogstickdisable_rightonly.skprx", "disables right analog stick"},
    {"analogstickdisable_righttoleft.skprx", "right stick acts as left stick"},
    {"analogstickdisable_swapleftandright.skprx", "swaps left and right sticks"},
    {"autoboot.suprx", "auto-launch an app at boot"},
    {"bettertrackplug.skprx", "playtime tracker"},
    {"bootsound.suprx", "custom boot jingle (wav)"},
    {"capunlocker.skprx", "unlocks cache0: and 4th core"},
    {"catherinefbhd.suprx", "catherine full body hd patch"},
    {"cidspoof.skprx", "spoofs console id"},
    {"cpuaffinitychanger.skprx", "system apps use all user cores"},
    {"custom_boot_splash.skprx", "custom boot splash (needs enso)"},
    {"custom_warning.suprx", "custom boot warning message"},
    {"dejavu.skprx", "ram snapshot and restore"},
    {"dolcepolce.suprx", "removes pstv blacklist checks"},
    {"download_enabler.suprx", "download files via browser"},
    /* [C] catalog text is MiniVitaTV's, not the DS3 driver's */
    {"ds3.skprx", "dualshock 3 controller support"},
    {"ds34vita.skprx", "ds3/ds4/dualsense support"},
    {"ds3vita.skprx", "control your vita with a dualshock 3"},
    {"ds4touch.skprx", "ds4 touchpad support"},
    {"ds4vita.skprx", "control your vita with a dualshock 4"},
    {"dsmotion.skprx", "dualshock motion for pstv"},
    {"dsmotion.suprx", "dualshock motion for pstv"},
    {"dt2s_statusbar.suprx", "double-tap status bar to sleep"},
    {"dualsense-driver.skprx", "dualsense controller support"},
    {"ef2uma.suprx", "maps psp go ef0 to uma0"},
    {"ef2xmc.suprx", "maps psp go ef0 to xmc0"},
    {"emergencymount.skprx", "hold triangle at boot to mount usb"},
    {"fd_fix.skprx", "fixes file descriptors after suspend"},
    {"freepsm.suprx", "debug psm in-app purchases"},
    {"fruitpeel.suprx", "custom lockscreen wallpaper"},
    {"ftpeverywhere.suprx", "background ftp server"},
    {"gr544p.suprx", "gravity rush 544p patch"},
    /* [C] not in the catalog; from the HENkaku plugin source */
    {"henkaku.suprx", "settings menu, version and update spoof"},
    {"hidkeyboard.skprx", "vita as a usb keyboard"},
    {"honripper.suprx", "extracts sony reader .mnh books"},
    {"infinitenet.suprx", "stops games disabling net features"},
    /* [C] dependency; catalog has no standalone entry */
    {"ioplus.skprx", "more io operations for userland"},
    /* [C] not in the catalog; community knowledge */
    {"itls.skprx", "tls 1.2 support"},
    {"jav.suprx", "auto speaker/headphone volume"},
    {"kubridge.skprx", "kernel-user bridge module"},
    /* [C] dependency; catalog has no standalone entry */
    {"kuio.skprx", "user modules access ux0:data i/o"},
    /* [C] not in the catalog; community knowledge */
    {"libshacccg.suprx", "shader compiler for vitagl"},
    {"livesky.suprx", "redirects livetweet to bluesky"},
    {"lolita444.skprx", "sets clocks to 444 mhz"},
    {"lolita500.skprx", "sets clocks to maximum"},
    /* [C] catalog text is garbled; this is the readable form */
    {"lowmemmode.suprx", "launches system apps with more ram"},
    {"macspoofer.suprx", "spoofs mac address"},
    {"mafuinput.skprx", "records and replays input"},
    {"minivitatv.skprx", "local multiplayer, up to 4 pads"},
    {"modalvol.suprx", "system volume control in game"},
    {"music_premium.skprx", "background music in any game"},
    {"noavls.skprx", "disables avls on forced consoles"},
    {"noled.skprx", "turns off home button led"},
    {"nolockscreen.suprx", "disables the lockscreen"},
    {"nonpdrm.skprx", "bypass drm on vita content"},
    {"nonpdrm_un.skprx", "bypass drm, unofficial fork"},
    {"nopowerlimits.skprx", "removes some scepower restrictions"},
    {"nopsmdrm.skprx", "bypass drm on psm game content"},
    {"nopspemudrm_kern.skprx", "bypass psp/ps1 drm checks"},
    {"nopspemudrm_kern_mod.skprx", "bypass psp/ps1 drm (ark)"},
    {"nopspemudrm_user.suprx", "bypass psp/ps1 drm checks"},
    {"nopspemudrm_user_mod.suprx", "bypass psp/ps1 drm (ark)"},
    {"nosharedfb.suprx", "fixes framebuffer after suspend"},
    {"nosleep.skprx", "disables the sleep timer"},
    {"notrophymsg.suprx", "hides cross-account trophy hint"},
    {"notrpdrm.suprx", "enables trophies for homebrew"},
    {"p4goldenhd_1280x720.suprx", "persona 4 golden 720p patch"},
    {"p4goldenhd_1920x1080.suprx", "persona 4 golden 1080p patch"},
    {"pasteunlock.suprx", "unlocks system copy/paste"},
    {"pngshot.suprx", "screenshot to png"},
    {"pocketstationunlocker.skprx", "pocketstation on all ps1 classics"},
    {"ps4linkcontrols.suprx", "remote play button config"},
    {"psonescrot.skprx", "proper ps1 screenshots"},
    {"psp2wpp.suprx", "custom home background wave"},
    {"pspemu-colour-crunch.skprx", "pspemu colour space (vita 2000)"},
    {"psvshell.skprx", "overclocking with 3 gui modes"},
    {"psvshell_mod.skprx", "overclocking, raangi mod"},
    {"psvshellplus_kernel.skprx", "overclock and info hud"},
    {"psvshellplus_shell.suprx", "overclock and info hud"},
    {"qascreenshot.suprx", "native qa screenshot"},
    {"quickmenuplus.suprx", "power/volume quick menu"},
    {"quickmenureborn.suprx", "quick menu widget library"},
    {"quietshot.suprx", "silences the shutter sound"},
    {"reauth.skprx", "playstation store sign-in"},
    {"ref00d.skprx", "runs programs that do not decrypt"},
    {"rehomearcade.suprx", "unlocks ps home arcade games"},
    {"remapsv.suprx", "button and touch remapping"},
    {"remapsv2.suprx", "button and touch remapping"},
    {"repatch.skprx", "mods encrypted games"},
    {"repatch_4.skprx", "older repatch build"},
    {"repatch_ex.skprx", "repatch reloaded"},
    {"rerescaler.suprx", "shader upscaling for non-native res"},
    {"resavedata.skprx", "unencrypted savedata redirect"},
    {"rescreeny.suprx", "screenshots in per-game folders"},
    {"reskype.suprx", "enables skype on 3.65+"},
    {"restorepsmdev.suprx", "restores psm dev apps"},
    {"revita.skprx", "input remapping and macros"},
    /* [C] catalog text is a joke; this is the function */
    {"scorehax.suprx", "score cheat for any game"},
    {"sharpscale.skprx", "native 960x544 hdmi for pstv"},
    {"shellbat.suprx", "battery percent in status bar"},
    {"shellbat_mod.suprx", "battery percent and ip in status bar"},
    {"shellsecbat.suprx", "battery, seconds and drive state"},
    {"showinfo.suprx", "shows title id, name and version"},
    {"showinfolite.suprx", "shows title id, name and version"},
    {"staybright.skprx", "no auto-dim, keeps auto-suspend"},
    {"storagemgr.skprx", "storage remap, sd2vita"},
    {"sysident.suprx", "system info in settings"},
    {"trimpose.suprx", "transparent power menu"},
    {"trophax2.0.suprx", "universal trophy unlocker"},
    {"trophyshot.suprx", "screenshot on trophy unlock"},
    {"turbopad.suprx", "rapid fire for any button"},
    {"tvikey.skprx", "pstv mouse/keyboard driver"},
    {"udcd_uvc.skprx", "usb video capture"},
    {"udcd_uvc_lcd_off.skprx", "usb video, lcd off"},
    {"udcd_uvc_oled_off.skprx", "usb video, oled off"},
    {"usbdisable.suprx", "hides the usb connect dialog"},
    {"usbmc.skprx", "usb storage as memory card"},
    {"usbmcm.skprx", "usb storage as memory card (psvsd)"},
    {"vflux.suprx", "screen colour temperature"},
    {"vgi.suprx", "game info overlay"},
    {"videodebug.skprx", "video app debug mode"},
    {"viimote.skprx", "control your vita with a wiimote"},
    {"vita3g-disable.suprx", "disables 3g, reports wifi-only"},
    {"vitabright.skprx", "custom gamma table, dimmer screen"},
    {"vitabtfix.skprx", "fixes silent bluetooth audio"},
    {"vitacheat.skprx", "cheats for 3.65/3.68 games"},
    {"vitacheat360.skprx", "cheats for 3.60 games"},
    {"vitacompanion.suprx", "homebrew dev companion"},
    {"vitacompanion_kernel.skprx", "homebrew dev companion, kernel"},
    {"vitacontrol.skprx", "bluetooth controller support"},
    {"vitagrafix.suprx", "game resolution and fps patches"},
    {"vitanearestneighbour.suprx", "nearest-neighbour texture filter"},
    {"vitapad.skprx", "vita as a usb gamepad"},
    {"vitapresence.skprx", "discord rich presence"},
    {"vitaqmbluetooth.skprx", "quick menu bluetooth reconnect"},
    {"vitastick.skprx", "vita as a usb controller"},
    {"vixen.skprx", "x-input gamepad driver"},
    {"vollightbarctrlwithds4.skprx", "volume via ds4, lightbar battery"},
    {"vsh.suprx", "in-game vsh menu"},
    {"vshpatch.skprx", "patches system applications"},
    {"wdnf.suprx", "disables front touchscreen"},
    {"wdnr.suprx", "disables rear touchpad"},
    {"x1vita.skprx", "xbox one controller support"},
    /* [C] not in the catalog; from the YAMT README */
    {"yamt.suprx", "storage remount settings gui"},
};

#define KNOWN_COUNT ((int)(sizeof(KNOWN) / sizeof(KNOWN[0])))

/* Case-insensitive compare of a slice, which is not NUL-terminated. */
/*
 * Map a section header to its group. `line` includes the leading '*', which is
 * how taiHEN marks a load target. Section names are compared exactly, because
 * taiHEN uses strcmp; `*main` is lowercase and `*ALL` is uppercase. Unknown
 * targets are title IDs (D51).
 */
static void add_plugin(PluginList *list, const char *path, PluginSection section)
{
    const char *base = tai_base_name(path);
    if (*base == '\0')
        return;

    /* One row per unique path, not per line and not per basename. A plugin
     * listed in several sections keeps the most privileged one: KERNEL
     * outranks SYSTEM, which outranks a system app, ALL, and a single title
     * (D51). Checking this before the ceiling also means a repeated line is
     * not a lost plugin. */
    for (int i = 0; i < list->count; ++i) {
        if (tai_ieq(list->items[i].path, path)) {
            if (section < list->items[i].section)
                list->items[i].section = section;
            return;
        }
    }

    if (list->count >= PLUGINS_MAX) {
        /* The config listed more than this array can hold. Say so; a silent
         * drop is what made the 12-slot version look like a parser failure. */
        list->truncated = 1;
        return;
    }

    PluginInfo *p = &list->items[list->count];

    tai_copy(p->name, sizeof(p->name), base);
    tai_copy(p->path, sizeof(p->path), path);
    p->desc[0] = '\0';
    p->section = section;

    for (int k = 0; k < KNOWN_COUNT; ++k) {
        if (tai_ieq(KNOWN[k].name, p->name)) {
            tai_copy(p->desc, sizeof(p->desc), KNOWN[k].desc);
            break;
        }
    }

    ++list->count;
}

int plugins_parse(const char *text, PluginList *list)
{
    if (text == NULL || list == NULL)
        return 0;

    int added = 0;
    const char *p = text;
    PluginSection section = PLUGIN_SECTION_TITLE; /* replaced by the first header */
    int have_section = 0;
    int halted = 0;

    while (*p != '\0') {
        const char *line = p;

        while (*p != '\0' && *p != '\n')
            ++p;

        size_t len = (size_t)(p - line);
        if (*p == '\n')
            ++p;

        /* Trim trailing CR and spaces; the Vita may hand back either. */
        while (len > 0 && (line[len - 1] == '\r' || line[len - 1] == ' ' ||
                           line[len - 1] == '\t'))
            --len;

        while (len > 0 && (*line == ' ' || *line == '\t')) {
            ++line;
            --len;
        }

        if (len == 0 || *line == '#')
            continue;

        /* A section header selects the group the following paths load into.
         * `*!Section` is a halt point: taiHEN stops parsing there, so every
         * later path is not reached and is not part of the activated set. */
        if (*line == '*') {
            if (len >= 2 && line[1] == '!') {
                halted = 1;
                have_section = 1;
                continue;
            }

            section = tai_section_from_header(line, len);
            have_section = 1;
            continue;
        }

        if (halted)
            continue;

        /*
         * A path before any section header has no load target. taiHEN ignores
         * it; filing it under a guess would invent a group. The config is
         * invalid, so HOME shows no counts rather than a partial set.
         */
        if (!have_section) {
            list->config_invalid = 1;
            continue;
        }

        char path[PLUGIN_PATH_MAX];
        if (len >= sizeof(path))
            continue; /* too long: a truncated path names a different file */

        memcpy(path, line, len);
        path[len] = '\0';

        if (!tai_is_plugin_path(path))
            continue;

        int before = list->count;
        add_plugin(list, path, section);
        if (list->count != before)
            ++added;
    }

    return added;
}

const char *plugins_desc_for(const char *name)
{
    if (name == NULL)
        return "";

    for (int k = 0; k < KNOWN_COUNT; ++k) {
        if (tai_ieq(KNOWN[k].name, name))
            return KNOWN[k].desc;
    }

    return "";
}

void plugins_scan(void)
{
    memset(&g_plugins, 0, sizeof(g_plugins));

    static char buf[CONFIG_CAP];

    /*
     * taiHEN opens ux0: first and falls back to ur0: only when ux0: cannot be
     * opened. A readable empty ux0: file is still the active config, and a
     * too-large file is active but unreadable; neither falls through to ur0:.
     */
    for (int i = 0; i < CONFIG_COUNT; ++i) {
        int n = 0;
        PlatformReadStatus status = platform_read_config(
            CONFIGS[i], buf, (int)sizeof(buf), &n);

        if (status == PLATFORM_READ_OK) {
            g_plugins.config_found = 1;
            if (n > 0)
                plugins_parse(buf, &g_plugins);
            break;
        }

        if (status == PLATFORM_READ_TOO_LARGE) {
            g_plugins.config_too_large = 1;
            break;
        }
    }

    /*
     * A config with a path before the first section is invalid. taiHEN does not
     * load a partial list, so HOME shows no counts rather than a subset.
     */
    if (g_plugins.config_invalid) {
        g_plugins.count = 0;
        g_plugins.truncated = 0;
    }

    /*
     * Keep only what is actually on disk. A path listed in config.txt whose
     * file is missing is skipped by the loader, so counting it would report a
     * plugin that is not running.
     */
    int kept = 0;

    for (int i = 0; i < g_plugins.count; ++i) {
        if (!platform_file_exists(g_plugins.items[i].path))
            continue;

        if (kept != i)
            g_plugins.items[kept] = g_plugins.items[i];

        ++kept;
    }

    g_plugins.count = kept;
}
