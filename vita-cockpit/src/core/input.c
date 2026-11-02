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

InputEvent input_update(InputState *state, int up, int down, int left, int right,
                        int accept, int cancel, int modal, float dt)
{
    if (state == NULL)
        return INPUT_NONE;

    InputEvent event = INPUT_NONE;

    if (modal) {
        if (pressed(accept, state->accept))
            event = INPUT_ACCEPT;
        else if (pressed(cancel, state->cancel))
            event = INPUT_CANCEL;
    } else if (pressed(left, state->left)) {
        event = INPUT_TAB_LEFT;
    } else if (pressed(right, state->right)) {
        event = INPUT_TAB_RIGHT;
    } else if (pressed(accept, state->accept)) {
        event = INPUT_ACCEPT;
    } else if (pressed(cancel, state->cancel)) {
        event = INPUT_CANCEL;
    } else if (pressed(up, state->up)) {
        event = INPUT_UP;
        state->repeat = 0.0f;
    } else if (pressed(down, state->down)) {
        event = INPUT_DOWN;
        state->repeat = 0.0f;
    } else if (up || down) {
        state->repeat += dt;
        if (state->repeat >= INPUT_REPEAT_DELAY) {
            event = up ? INPUT_UP : INPUT_DOWN;
            state->repeat = INPUT_REPEAT_DELAY - INPUT_REPEAT_RATE;
        }
    } else {
        state->repeat = 0.0f;
    }

    state->up = up;
    state->down = down;
    state->left = left;
    state->right = right;
    state->accept = accept;
    state->cancel = cancel;

    return event;
}
