#ifndef COCKPIT_UI_MODAL_H
#define COCKPIT_UI_MODAL_H

#include "ui/ui_state.h"

void modal_open_confirm_titled(UiState *ui, const char *title, int row,
                               int mode, const char *text);
void modal_open_error_titled(UiState *ui, const char *title, const char *text);
void modal_open_info(UiState *ui, const char *text);
void modal_open_info_titled(UiState *ui, const char *title, const char *text);
void modal_open_receipt(UiState *ui, const char *title, const char *text);
void modal_open_session(UiState *ui, const char *text, const char *const *choices,
                        int count);
void modal_open_restart(UiState *ui, const char *text);
void modal_draw(const UiState *ui);

#endif
