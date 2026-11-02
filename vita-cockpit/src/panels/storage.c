#include "panels/panel.h"

#include <stdio.h>

#include "core/metrics.h"

/*
 * STORAGE — what each mount holds, and how full it is.
 *
 * The meter is drawn only from a real reading. A card that is absent shows
 * "--" and a dimmed empty track, which is visibly different from a card that
 * is present and empty.
 *
 * Each volume is a two-line block: the readout across the panel's full width,
 * then the meter beneath it. The meter used to sit inline at x=320, which left
 * the value stranded mid-panel and forced the two rows to share a 34 px stride.
 * A full-width track under its own value reads as one block and gives the row
 * the height the grid needs (D48).
 */

#define STORAGE_METER_H 10.0f
#define STORAGE_METER_DY (LINE_H(PX_VALUE) + SP_2)
#define STORAGE_ROW0_Y PANEL_BODY_Y
#define STORAGE_ROW_H (STORAGE_METER_DY + STORAGE_METER_H + SP_4)

#define VALUE_BUF 24

/* Room for "used/total UNIT": two VALUE_BUF numbers, a separator and a unit. */
#define JOINED_BUF (VALUE_BUF * 3)

static const char *const MOUNT_LABELS[STORAGE_COUNT] = {"UX0", "UMA0"};

/* A volume at or above this many MB is shown in GB. */
#define MB_PER_GB 1024

/*
 * One unit per row, chosen from the volume's own size: GB with one decimal for
 * a card, whole MB for anything smaller. A fixed GB unit would render a 512 MB
 * volume as "0.5", losing the precision that matters at that size.
 */
static int mb_to_gb_tenths(int mb)
{
    return (mb * 10 + MB_PER_GB / 2) / MB_PER_GB;
}

static void fmt_pair(char *buf, size_t n, int used_mb, int total_mb)
{
    if (total_mb >= MB_PER_GB) {
        char used[VALUE_BUF];
        char total[VALUE_BUF];

        theme_fmt_tenths(used, sizeof(used), mb_to_gb_tenths(used_mb));
        theme_fmt_tenths(total, sizeof(total), mb_to_gb_tenths(total_mb));
        snprintf(buf, n, "%s/%s GB", used, total);
    } else {
        snprintf(buf, n, "%d/%d MB", used_mb, total_mb);
    }
}

static void draw_row(Rect r, int i, uint32_t accent)
{
    const StorageSample *s = &g_metrics.storage[i];
    float y = r.y + STORAGE_ROW0_Y + (float)i * STORAGE_ROW_H;

    char joined[JOINED_BUF];

    if (s->valid) {
        fmt_pair(joined, sizeof(joined), s->used_mb, s->total_mb);
    } else if (s->error < 0) {
        /*
         * The mount answered and was refused. A ten-digit SCE code is not
         * readable on a HUD, so show the errno byte the port decoded: "ERR 30"
         * is the permission refusal, "ERR 13" would be ENODEV.
         */
        snprintf(joined, sizeof(joined), "ERR %02X", s->errno_byte);
    } else {
        /*
         * No volume in the slot. "N/A" rather than "--": a mount that is not
         * present is a different statement from a metric that could not be
         * read, and it must never render as an empty row (D41).
         */
        snprintf(joined, sizeof(joined), "N/A");
    }

    theme_readout(MOUNT_LABELS[i], joined, s->valid, r.x + PANEL_INSET, y,
                  r.w - 2.0f * PANEL_INSET);

    float fill = 0.0f;
    if (s->valid && s->total_mb > 0)
        fill = (float)s->used_mb / (float)s->total_mb;

    theme_meter(r.x + PANEL_INSET, y + STORAGE_METER_DY,
                r.w - 2.0f * PANEL_INSET, STORAGE_METER_H, fill, s->valid,
                accent);
}

static void draw_storage(const Panel *self, Rect r)
{
    for (int i = 0; i < STORAGE_COUNT; ++i)
        draw_row(r, i, self->accent);
}

const Panel panel_storage = {"STORAGE", C_CYAN, draw_storage};
