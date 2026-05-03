/*===========================================================================
 * exm_display.h - EXM graphical display for Wordle LX
 *===========================================================================*/

#ifndef EXM_DISPLAY_H
#define EXM_DISPLAY_H

#include "game.h"

/*---------------------------------------------------------------------------
 * Layout constants (pixels, 640x190 usable area)
 *
 *  |<--  BOARD AREA (x=10..225)  -->|<----- RIGHT PANEL (x=242..630) ----->|
 *  |        (title bar above entire app window)                             |
 *  |                                |  "Remaining Letters"                  |
 *  |   [tile][tile][tile][tile][tile]|  Q W E R T Y U I O P                |
 *  |   [tile][tile][tile][tile][tile]|   A S D F G H J K L                 |
 *  |   ...  (6 rows)                |    Z X C V B N M                     |
 *  |                                |  ------------------------------------ |
 *  |                                |  Legend                               |
 *  |                                |  ------------------------------------ |
 *  |                                |  Message line 1 (centered)            |
 *  |                                |  Message line 2 (centered)            |
*  |=== F-key bar: F1=New  F8=About  F9=Help  F10=Quit  (drawn by LHAPI) ===|
 *---------------------------------------------------------------------------*/

/* Board area */
#define BOARD_X       10   /* leftmost pixel of first tile column */
#define BOARD_Y       20   /* topmost pixel of first tile row */
#define BOARD_AREA_W  (5 * (TILE_W + TILE_HG) - TILE_HG)  /* 216 px */
#define TILE_W        40   /* tile width in pixels */
#define TILE_H        22   /* tile height in pixels */
#define TILE_HG        4   /* horizontal gap between tiles */
#define TILE_VG        3   /* vertical gap between rows */

/* Right panel */
#define TITLE_BAR_H   10   /* CAP title bar height used by DRAW_TITLE */
#define DIVIDER_X    234   /* vertical divider x coordinate */
#define KBOARD_X     242   /* right panel left edge */
#define RIGHT_LABEL_Y 17   /* "Remaining Letters" label y (below title bar) */

/* Per-letter indicator boxes */
#define KB_BOX_W      22   /* indicator width */
#define KB_BOX_H      14   /* indicator height */
#define KB_GAP         2   /* gap between indicators */
#define KBOARD_Y      29   /* top of first keyboard row (below label) */

/* Message area in right panel, below legend separator.
   Separator at RIGHT_MSG_Y-3; line 1 at RIGHT_MSG_Y+1; line 2 at RIGHT_MSG_Y+11. */
#define RIGHT_MSG_Y  142

/* Right panel usable width (KBOARD_X to x=630) */
#define RIGHT_PANEL_W  (630 - KBOARD_X)   /* 388 px */

/* Graphics mode */
#define GFX_MODE    0x06  /* G_CGAGRAPH: 640x200 CGA-compatible */

/*---------------------------------------------------------------------------
 * Drawing functions
 *---------------------------------------------------------------------------*/

/* Detect and cache available fonts. Call once before any drawing. */
void exm_init_fonts(void);

/* Draw static chrome: divider, panel header, legend.
   Called on DRAW_FRAME (full redraws). */
void exm_draw_chrome(void);

/* Draw all 6x5 game tiles. */
void exm_draw_board(const GameState far *gs);

/* Draw A-Z remaining-letters panel. */
void exm_draw_keyboard(const GameState far *gs);

/* Draw one centered feedback message line in the message area (clears both lines). */
void exm_draw_message(const char far *msg);

/* Draw two centered lines in the message area. */
void exm_draw_message2(const char far *line1, const char far *line2);

/* Draw three centered lines in the message area. */
void exm_draw_message3(const char far *line1, const char far *line2, const char far *line3);

/* Draw the large in-game help dialog overlay. */
void exm_draw_help_dialog(void);

/* Draw the in-game about dialog overlay. */
void exm_draw_about_dialog(void);

/* Redraw only the active input row tiles — fast path for letter entry. */
void exm_draw_active_row(const GameState far *gs);

#endif /* EXM_DISPLAY_H */
