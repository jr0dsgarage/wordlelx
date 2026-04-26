#ifndef DISPLAY_H
#define DISPLAY_H

#include "game.h"

typedef struct {
    void (*init)(void);
    void (*draw_board)(const GameState *gs);
    void (*draw_keyboard)(const GameState *gs);
    void (*draw_message)(const char *msg);
    void (*cleanup)(void);
} Display;

/* Implemented in text_mode.c */
void text_mode_init_display(Display *d);

#endif
