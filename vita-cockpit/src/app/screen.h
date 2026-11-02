#ifndef COCKPIT_APP_SCREEN_H
#define COCKPIT_APP_SCREEN_H

/*
 * One complete frame: header, every panel's chrome and contents, the sweep.
 *
 * Split out of app.c so the host preview renders the shipped composition
 * instead of a copy of it (D26). This file makes no SDK call and reads no
 * clock, so tools/preview can link it unchanged.
 *
 * `scroll` is the canvas offset in pixels; the app owns it and the panels never
 * see it, because their rects are translated here (D45).
 */
void screen_draw(float t, float scroll);

#endif
