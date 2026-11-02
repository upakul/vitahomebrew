#include "core/history.h"

#include <string.h>

void history_reset(History *h)
{
    if (h == NULL)
        return;
    memset(h, 0, sizeof(*h));
}

void history_push(History *h, float value)
{
    if (h == NULL)
        return;

    h->v[h->head] = value;
    h->head = (h->head + 1) % HISTORY_CAP;

    if (h->len < HISTORY_CAP)
        ++h->len;
}

float history_at(const History *h, int i)
{
    if (h == NULL || i < 0 || i >= h->len)
        return 0.0f;

    int idx = h->head - 1 - i;
    while (idx < 0)
        idx += HISTORY_CAP;

    return h->v[idx];
}

void history_range(const History *h, float *lo, float *hi)
{
    if (h == NULL || h->len == 0) {
        if (lo != NULL)
            *lo = 0.0f;
        if (hi != NULL)
            *hi = 0.0f;
        return;
    }

    float lowest = history_at(h, 0);
    float highest = 0.0f;

    for (int i = 0; i < h->len; ++i) {
        float v = history_at(h, i);

        if (v < lowest)
            lowest = v;
        if (v > highest)
            highest = v;
    }

    if (lo != NULL)
        *lo = lowest < highest ? lowest : highest;
    if (hi != NULL)
        *hi = highest;
}
