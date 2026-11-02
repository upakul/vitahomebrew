#ifndef COCKPIT_UI_UI_STATE_H
#define COCKPIT_UI_UI_STATE_H

/*
 * The screen's navigation state. The app owns it; screen_draw and the panels
 * read it. Keeping it out of the panels is what lets HOME and PLUGINS keep
 * independent scroll positions (KTD1).
 */

typedef enum {
    UI_TAB_HOME = 0,
    UI_TAB_PLUGINS,
    UI_TAB_COUNT
} UiTab;

typedef enum {
    UI_MODAL_NONE = 0,
    UI_MODAL_CONFIRM,
    UI_MODAL_ERROR,
    UI_MODAL_INFO,
    UI_MODAL_RECEIPT,
    UI_MODAL_SESSION,
    UI_MODAL_RESTART
} UiModal;

#define UI_MODAL_MODE_RESTORE 2
#define UI_MODAL_CHOICES_MAX 3
#define UI_MODAL_CHOICE_TEXT 32
#define UI_MODAL_TITLE_TEXT_MAX 34

typedef struct {
    UiTab tab;
    float home_scroll;
    float plugins_scroll;
    int selection; /* row index in g_editor */

    UiModal modal;
    int modal_row;
    int modal_mode;
    int modal_choice;    int modal_choice_count;
    char modal_choices[UI_MODAL_CHOICES_MAX][UI_MODAL_CHOICE_TEXT];
    char modal_title[64];
    char modal_body[512];
} UiState;

#endif
