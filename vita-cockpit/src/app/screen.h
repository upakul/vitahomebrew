#ifndef COCKPIT_APP_SCREEN_H
#define COCKPIT_APP_SCREEN_H

#include "ui/ui_state.h"

/*
 * One complete frame: header, the active tab, the pinned status strip, the
 * modal, and the sweep. Split out of app.c so the host preview renders the
 * shipped composition (D26); this file makes no SDK call and reads no clock.
 */
void screen_draw(float t, const UiState *ui);

#endif
