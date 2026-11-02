#include "core/input.h"

#include <string.h>

void input_reset(InputState *state)
{
    if (state != NULL)
        memset(state, 0, sizeof(*state));
}

static int pressed(int now, int before)
{
    return now && !before;
}

static InputEvent repeat_direction(InputState *state, int up, int down, float dt)
{
    if (pressed(up, state->up)) {
        state->repeat = 0.0f;
        return INPUT_UP;
    }

    if (pressed(down, state->down)) {
        state->repeat = 0.0f;
        return INPUT_DOWN;
    }

    if (up || down) {
        state->repeat += dt;
        if (state->repeat >= INPUT_REPEAT_DELAY) {
            state->repeat = INPUT_REPEAT_DELAY - INPUT_REPEAT_RATE;
            return up ? INPUT_UP : INPUT_DOWN;
        }

        return INPUT_NONE;
    }

    state->repeat = 0.0f;
    return INPUT_NONE;
}

InputEvent input_update(InputState *state, int up, int down, int left, int right,
                        int accept, int cancel, int options,
                        InputContext context, float dt)
{
    if (state == NULL)
        return INPUT_NONE;

    InputEvent event = INPUT_NONE;

    if (context != INPUT_CONTEXT_APP) {
        if (pressed(accept, state->accept)) {
            event = INPUT_ACCEPT;
        } else if (pressed(cancel, state->cancel)) {
            event = INPUT_CANCEL;
        } else if (context == INPUT_CONTEXT_MODAL_LIST) {
            event = repeat_direction(state, up, down, dt);
        } else {
            state->repeat = 0.0f;
        }
    } else if (pressed(left, state->left)) {
        event = INPUT_TAB_LEFT;
    } else if (pressed(right, state->right)) {
        event = INPUT_TAB_RIGHT;
    } else if (pressed(accept, state->accept)) {
        event = INPUT_ACCEPT;
    } else if (pressed(cancel, state->cancel)) {
        event = INPUT_CANCEL;
    } else if (pressed(options, state->options)) {
        event = INPUT_OPTIONS;
    } else {
        event = repeat_direction(state, up, down, dt);
    }

    state->up = up;
    state->down = down;
    state->left = left;
    state->right = right;
    state->accept = accept;
    state->cancel = cancel;
    state->options = options;

    return event;
}
