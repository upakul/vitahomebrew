#ifndef COCKPIT_CORE_INPUT_H
#define COCKPIT_CORE_INPUT_H

/*
 * Pad edges and repeats, independent of the Vita SDK.
 *
 * The old loop integrated a held direction every frame, which is fine for a
 * scroll but wrong for a tab or a toggle: holding Cross would write sixty times
 * a second, and a modal would accept its own opening press. This module turns
 * the pad into at most one event per frame and owns the repeat cadence.
 */

typedef enum {
    INPUT_NONE = 0,
    INPUT_TAB_LEFT,
    INPUT_TAB_RIGHT,
    INPUT_UP,
    INPUT_DOWN,
    INPUT_ACCEPT,
    INPUT_CANCEL,
    INPUT_OPTIONS
} InputEvent;

typedef enum {
    INPUT_CONTEXT_APP = 0,
    INPUT_CONTEXT_MODAL,
    INPUT_CONTEXT_MODAL_LIST,
} InputContext;

typedef struct {
    int up;
    int down;
    int left;
    int right;
    int accept;
    int cancel;
    int options;
    float repeat;
} InputState;

#define INPUT_REPEAT_DELAY 0.42f
#define INPUT_REPEAT_RATE 0.09f

void input_reset(InputState *state);

/*
 * `context` selects what the pad means: the app, a text modal (accept/cancel
 * only), or a list modal (accept/cancel plus up/down). The opening press is
 * already in the previous state, so the modal cannot accept itself next frame.
 */
InputEvent input_update(InputState *state, int up, int down, int left, int right,
                        int accept, int cancel, int options,
                        InputContext context, float dt);

#endif
