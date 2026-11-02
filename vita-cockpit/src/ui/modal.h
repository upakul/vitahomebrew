#ifndef COCKPIT_UI_MODAL_H
#define COCKPIT_UI_MODAL_H

#include "ui/ui_state.h"

void modal_open_confirm(UiState *ui, int row, int mode, const char *text);
void modal_open_error(UiState *ui, const char *text);
void modal_open_info(UiState *ui, const char *text);
void modal_draw(const UiState *ui);

#endif
