/*===========================================================================
 * exm_display.h - EXM graphical display for Wordle LX
 *===========================================================================*/

#ifndef EXM_DISPLAY_H
#define EXM_DISPLAY_H

#include "game.h"

/*---------------------------------------------------------------------------
 * Layout constants (pixels, 640x190 usable area)
 *
 *  |<-------  BOARD AREA (x=10..229)  ------->|<- RIGHT PANEL (x=242..630) ->|
 *  |                                           |  "Wordle LX"                 |
 *  |                                           |  "Remaining Letters"         |
 *  |   [tile][tile][tile][tile][tile]           |  Q W E R T Y U I O P        |
 *  |   [tile][tile][tile][tile][tile]           |   A S D F G H J K L         |
 *  |   ...  (6 rows)                           |    Z X C V B N M             |
 *  |                                           |  Legend                      |
 *  |--- message area -------------------------------------------------|
 *  |=== F-key bar: F1=Quit  F2=New  F10=Submit  (drawn by LHAPI) ====|
 *---------------------------------------------------------------------------*/

/* Board area */
#define BOARD_X      10   /* leftmost pixel of first tile column */
#define BOARD_Y      16   /* topmost pixel of first tile row */
#define TILE_W       40   /* tile width in pixels */
#define TILE_H       22   /* tile height in pixels */
#define TILE_HG       4   /* horizontal gap between tiles */
#define TILE_VG       3   /* vertical gap between rows */

/* Right panel */
#define DIVIDER_X    234  /* vertical divider x coordinate */
#define KBOARD_X     242  /* right panel left edge */
#define RIGHT_TITLE_Y  2  /* "Wordle LX" title y */
#define RIGHT_LABEL_Y 14  /* "Remaining Letters" label y */

/* Per-letter indicator boxes */
#define KB_BOX_W      22  /* indicator width */
#define KB_BOX_H      14  /* indicator height */
#define KB_GAP         2  /* gap between indicators */
#define KBOARD_Y      26  /* top of first keyboard row (below title+label) */

/* Message strip (above fkey bar) */
#define MSG_Y        165  /* feedback message y coordinate */

/* Graphics mode */
#define GFX_MODE    0x06  /* G_CGAGRAPH: 640x200 CGA-compatible */

/*---------------------------------------------------------------------------
 * Drawing functions
 *---------------------------------------------------------------------------*/

/* Draw static chrome: divider, panel header, legend.
   Called on DRAW_FRAME (full redraws). */
void exm_draw_chrome(void);

/* Draw all 6x5 game tiles. */
void exm_draw_board(const GameState far *gs);

/* Draw A-Z remaining-letters panel. */
void exm_draw_keyboard(const GameState far *gs);

/* Draw feedback message at MSG_Y. NULL clears the strip. */
void exm_draw_message(const char far *msg);

#endif /* EXM_DISPLAY_H */
