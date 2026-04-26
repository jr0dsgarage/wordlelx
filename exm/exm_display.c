/*===========================================================================
 * exm_display.c - Graphical renderer for Wordle LX EXM port
 *
 * Implements the visual layer using the HP 200LX COUGRAPH pixel graphics
 * library (G_Move, G_Rect, G_Text, G_FillMask) and the LHAPI CAP drawing
 * utilities (Rectangle, ClearRect, DrawTextShell).
 *
 * Screen: 640x200 monochrome CGA-compatible LCD.
 * All coordinates are absolute screen pixels, y=0 at top.
 *
 * Tile visual language (monochrome, no color):
 *   LS_CORRECT   - Solid black fill, letter drawn INVERTED (white on black)
 *   LS_MISPLACED - 50% diagonal hatch fill, dark letter
 *   LS_ABSENT    - 25% light dot fill, dark letter
 *   LS_UNKNOWN   - Outline only (empty tile), or thick border for active input
 *===========================================================================*/

#include "cap2.h"
#include "cougraph.h"
#include "exm_display.h"

/* String literals in code segment avoid segment-fixup on task swap */
static char ROM_VAR szTitle[]   = "Wordle LX";
static char ROM_VAR szLabel[]   = "Remaining Letters";
static char ROM_VAR szLegend1[] = "[X]=Correct";
static char ROM_VAR szLegend2[] = "[/]=Wrong Position";
static char ROM_VAR szLegend3[] = "[ ]=Not in word";

/* QWERTY layout row strings are defined locally in exm_draw_keyboard() */

/*---------------------------------------------------------------------------
 * Fill patterns for G_PATTERNFILL
 * Each 8-byte array is a repeating 8x8-pixel tile bitmask.
 *---------------------------------------------------------------------------*/

/* 50% hatch (alternating pixels) — used for LS_MISPLACED */
static unsigned char pat_hatch50[8] = {
    0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55
};

/* 25% light dot pattern — used for LS_ABSENT */
static unsigned char pat_dot25[8] = {
    0x88, 0x00, 0x22, 0x00, 0x88, 0x00, 0x22, 0x00
};

/*---------------------------------------------------------------------------
 * Internal helpers
 *---------------------------------------------------------------------------*/

/* Draw a thick outline rectangle (border_w pixels wide on each side).
   Uses G_Move/G_Rect so we can do multiple nested outlines.             */
static void draw_border(int x, int y, int w, int h, int border_w)
{
    int i;
    G_ColorSel(MAXCOLOR);  /* black pen */
    G_RepRule(G_FORCE);
    for (i = 0; i < border_w; i++) {
        G_Move(x+i, y+i);
        G_Rect(x+w-1-i, y+h-1-i, G_OUTLINE);
    }
}

/* Clear a rectangle to white (background) */
static void clear_rect_area(int x, int y, int w, int h)
{
    G_ColorSel(MINCOLOR);  /* white/clear */
    G_RepRule(G_FORCE);
    G_Move(x, y);
    G_Rect(x+w-1, y+h-1, G_SOLIDFILL);
}

/* Draw a single tile at (x,y) with given letter and state.
   active: non-zero means this tile is in the current active input row.  */
static void draw_tile(int x, int y, char letter, int state, int active)
{
    char buf[2];
    int  tx, ty;

    buf[0] = letter ? letter : ' ';
    buf[1] = '\0';

    /* Approximate text center: font is ~6 wide, ~9 tall in FONT_NORMAL */
    tx = x + (TILE_W - 6) / 2;
    ty = y + (TILE_H - 9) / 2;

    switch (state) {

    case LS_CORRECT:
        /* Solid black tile */
        G_ColorSel(MAXCOLOR);
        G_RepRule(G_FORCE);
        G_Move(x, y);
        G_Rect(x+TILE_W-1, y+TILE_H-1, G_SOLIDFILL);
        /* White letter via XOR over black fill */
        if (letter) {
            G_RepRule(G_XOR);
            G_Text(tx, ty, buf, 0);
            G_RepRule(G_FORCE);
        }
        break;

    case LS_MISPLACED:
        /* 50% hatch fill */
        G_FillMask(pat_hatch50);
        G_ColorSel(MAXCOLOR);
        G_RepRule(G_FORCE);
        G_Move(x, y);
        G_Rect(x+TILE_W-1, y+TILE_H-1, G_PATTERNFILL);
        /* Thin outline */
        G_Move(x, y);
        G_Rect(x+TILE_W-1, y+TILE_H-1, G_OUTLINE);
        /* Dark letter (FORCE so it's always visible over hatch) */
        if (letter) {
            G_RepRule(G_FORCE);
            G_ColorSel(MAXCOLOR);
            G_Text(tx, ty, buf, 0);
        }
        break;

    case LS_ABSENT:
        /* 25% light dot fill */
        G_FillMask(pat_dot25);
        G_ColorSel(MAXCOLOR);
        G_RepRule(G_FORCE);
        G_Move(x, y);
        G_Rect(x+TILE_W-1, y+TILE_H-1, G_PATTERNFILL);
        G_Move(x, y);
        G_Rect(x+TILE_W-1, y+TILE_H-1, G_OUTLINE);
        if (letter) {
            G_RepRule(G_FORCE);
            G_Text(tx, ty, buf, 0);
        }
        break;

    default: /* LS_UNKNOWN — empty or active-input tile */
        clear_rect_area(x, y, TILE_W, TILE_H);
        if (active && letter) {
            /* Active input: 2-pixel border to draw attention */
            draw_border(x, y, TILE_W, TILE_H, 2);
            G_ColorSel(MAXCOLOR);
            G_RepRule(G_FORCE);
            G_Text(tx, ty, buf, 0);
        } else {
            /* Empty future tile: thin outline only */
            G_ColorSel(MAXCOLOR);
            G_RepRule(G_FORCE);
            G_Move(x, y);
            G_Rect(x+TILE_W-1, y+TILE_H-1, G_OUTLINE);
        }
        break;
    }

    /* Restore defaults */
    G_ColorSel(MAXCOLOR);
    G_RepRule(G_FORCE);
}

/* Draw a small (KB_BOX_W x KB_BOX_H) keyboard indicator for state */
static void draw_kb_indicator(int x, int y, int state)
{
    switch (state) {
    case LS_CORRECT:
        G_ColorSel(MAXCOLOR);
        G_RepRule(G_FORCE);
        G_Move(x, y);
        G_Rect(x+KB_BOX_W-1, y+KB_BOX_H-1, G_SOLIDFILL);
        break;
    case LS_MISPLACED:
        G_FillMask(pat_hatch50);
        G_ColorSel(MAXCOLOR);
        G_RepRule(G_FORCE);
        G_Move(x, y);
        G_Rect(x+KB_BOX_W-1, y+KB_BOX_H-1, G_PATTERNFILL);
        G_Move(x, y);
        G_Rect(x+KB_BOX_W-1, y+KB_BOX_H-1, G_OUTLINE);
        break;
    case LS_ABSENT:
        G_FillMask(pat_dot25);
        G_ColorSel(MAXCOLOR);
        G_RepRule(G_FORCE);
        G_Move(x, y);
        G_Rect(x+KB_BOX_W-1, y+KB_BOX_H-1, G_PATTERNFILL);
        G_Move(x, y);
        G_Rect(x+KB_BOX_W-1, y+KB_BOX_H-1, G_OUTLINE);
        break;
    default:
        clear_rect_area(x, y, KB_BOX_W, KB_BOX_H);
        G_ColorSel(MAXCOLOR);
        G_RepRule(G_FORCE);
        G_Move(x, y);
        G_Rect(x+KB_BOX_W-1, y+KB_BOX_H-1, G_OUTLINE);
        break;
    }
    G_ColorSel(MAXCOLOR);
    G_RepRule(G_FORCE);
}

/*---------------------------------------------------------------------------
 * Public drawing functions
 *---------------------------------------------------------------------------*/

void exm_draw_chrome(void)
{
    int x = KBOARD_X;

    /* Vertical divider */
    G_ColorSel(MAXCOLOR);
    G_RepRule(G_FORCE);
    G_Move(DIVIDER_X, 0);
    G_Draw(DIVIDER_X, MSG_Y - 2);

    /* Horizontal separator above message strip */
    G_Move(0, MSG_Y - 2);
    G_Draw(639, MSG_Y - 2);

    /* Right panel header: game title (drawn twice for a bolder look) */
    G_Text(x + 4, RIGHT_TITLE_Y, szTitle, 0);
    G_RepRule(G_OR);
    G_Text(x + 5, RIGHT_TITLE_Y, szTitle, 0);
    G_RepRule(G_FORCE);

    /* "Remaining Letters" label */
    G_Text(x + 4, RIGHT_LABEL_Y, szLabel, 0);

    /* Legend */
    draw_kb_indicator(x, 85, LS_CORRECT);
    G_Text(x + KB_BOX_W + 3, 88, szLegend1, 0);

    draw_kb_indicator(x, 101, LS_MISPLACED);
    G_Text(x + KB_BOX_W + 3, 104, szLegend2, 0);

    draw_kb_indicator(x, 117, LS_ABSENT);
    G_Text(x + KB_BOX_W + 3, 120, szLegend3, 0);
}

void exm_draw_board(const GameState far *gs)
{
    int row, col, x, y;
    int is_active_row;
    char c;

    for (row = 0; row < MAX_GUESSES; row++) {
        is_active_row = (row == gs->num_guesses && !gs->over);

        for (col = 0; col < WORD_LEN; col++) {
            x = BOARD_X + col * (TILE_W + TILE_HG);
            y = BOARD_Y + row * (TILE_H + TILE_VG);

            if (row < gs->num_guesses) {
                /* Submitted guess — show scored letter */
                draw_tile(x, y,
                          gs->rows[row].letters[col],
                          (int)gs->rows[row].result[col],
                          0);
            } else if (is_active_row) {
                /* Current partial input */
                c = (col < gs->input_len) ? gs->input[col] : '\0';
                draw_tile(x, y, c, LS_UNKNOWN, 1);
            } else {
                /* Empty future row */
                draw_tile(x, y, '\0', LS_UNKNOWN, 0);
            }
        }
    }
}

void exm_draw_keyboard(const GameState far *gs)
{
    /*
     * QWERTY layout in three rows.
     * Each indicator box is KB_BOX_W wide; letter drawn inside via G_Text.
     * Rows are centered within the keyboard panel (KBOARD_X to 630).
     *
     *  Row 1: Q W E R T Y U I O P  (10 letters)
     *  Row 2:  A S D F G H J K L   (9  letters, indented half a box)
     *  Row 3:    Z X C V B N M     (7  letters, indented 1.5 boxes)
     */
    static const char ROM_VAR row1[] = "QWERTYUIOP";
    static const char ROM_VAR row2[] = "ASDFGHJKL";
    static const char ROM_VAR row3[] = "ZXCVBNM";

    int panel_w = 630 - KBOARD_X;  /* 388px */
    int box_step = KB_BOX_W + KB_GAP;

    /* x_base for each row, centering within panel */
    int x1_base = KBOARD_X + (panel_w - 10 * box_step + KB_GAP) / 2;
    int x2_base = KBOARD_X + (panel_w -  9 * box_step + KB_GAP) / 2;
    int x3_base = KBOARD_X + (panel_w -  7 * box_step + KB_GAP) / 2;

    int y1 = KBOARD_Y;
    int y2 = KBOARD_Y + KB_BOX_H + KB_GAP + 2;
    int y3 = KBOARD_Y + 2 * (KB_BOX_H + KB_GAP + 2);

    int i, letter_idx, state;
    char lbuf[2];
    int tx, ty;

    lbuf[1] = '\0';

    /* Row 1 */
    for (i = 0; i < 10; i++) {
        lbuf[0] = row1[i];
        letter_idx = lbuf[0] - 'A';
        state = gs->keyboard[letter_idx];
        draw_kb_indicator(x1_base + i * box_step, y1, state);
        /* Letter label centered in box */
        tx = x1_base + i * box_step + (KB_BOX_W - 6) / 2;
        ty = y1 + (KB_BOX_H - 9) / 2;
        if (state == LS_CORRECT) {
            G_RepRule(G_XOR);
            G_Text(tx, ty, lbuf, 0);
            G_RepRule(G_FORCE);
        } else {
            G_Text(tx, ty, lbuf, 0);
        }
    }

    /* Row 2 */
    for (i = 0; i < 9; i++) {
        lbuf[0] = row2[i];
        letter_idx = lbuf[0] - 'A';
        state = gs->keyboard[letter_idx];
        draw_kb_indicator(x2_base + i * box_step, y2, state);
        tx = x2_base + i * box_step + (KB_BOX_W - 6) / 2;
        ty = y2 + (KB_BOX_H - 9) / 2;
        if (state == LS_CORRECT) {
            G_RepRule(G_XOR);
            G_Text(tx, ty, lbuf, 0);
            G_RepRule(G_FORCE);
        } else {
            G_Text(tx, ty, lbuf, 0);
        }
    }

    /* Row 3 */
    for (i = 0; i < 7; i++) {
        lbuf[0] = row3[i];
        letter_idx = lbuf[0] - 'A';
        state = gs->keyboard[letter_idx];
        draw_kb_indicator(x3_base + i * box_step, y3, state);
        tx = x3_base + i * box_step + (KB_BOX_W - 6) / 2;
        ty = y3 + (KB_BOX_H - 9) / 2;
        if (state == LS_CORRECT) {
            G_RepRule(G_XOR);
            G_Text(tx, ty, lbuf, 0);
            G_RepRule(G_FORCE);
        } else {
            G_Text(tx, ty, lbuf, 0);
        }
    }
}

void exm_draw_message(const char far *msg)
{
    /* Clear message strip */
    clear_rect_area(0, MSG_Y, DIVIDER_X, 11);  /* 11px strip above fkey bar */

    if (msg) {
        G_ColorSel(MAXCOLOR);
        G_RepRule(G_FORCE);
        G_Text(BOARD_X, MSG_Y + 1, (char far *)msg, 0);
    }
}
