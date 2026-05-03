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
 *   LS_MISPLACED - 25% light dot fill (wrong position but in word)
 *   LS_ABSENT    - 50% diagonal hatch fill (not in word)
 *   LS_UNKNOWN   - Outline only (empty tile), or thick border for active input
 *===========================================================================*/

#include "cap2.h"
#include "cougraph.h"
#include "exm_display.h"

/* Plain DS strings — only read inside DRAW callbacks where DS is always valid */
static char szLabel[]   = "Remaining Letters";
static char szLegend1[] = "=Correct";
static char szLegend2[] = "=Wrong Position";
static char szLegend3[] = "=Not in word";

/* QWERTY layout row strings are defined locally in exm_draw_keyboard() */

/*---------------------------------------------------------------------------
 * Font management
 *
 * exm_init_fonts() detects the label/UI font once (for keyboard, chrome,
 * messages).  Tile letters use LHAPI DrawText with FONT_LARGE (16x12 px)
 * which is independent of the COUGRAPH font state.
 *
 * s_label_font_w drives all text-centering calculations.
 *---------------------------------------------------------------------------*/
static int       s_inited       = 0;
static void far *s_label_font   = NULL;
static int       s_label_font_w = 8;
static void far *s_tile_font    = NULL;  /* 16x12 COUGRAPH font for tile letters */

void exm_init_fonts(void)
{
    void far *fp;
    if (s_inited) return;
    s_inited = 1;

    s_tile_font = G_GetFont(FONT_LARGE);  /* 16x12 — direct bitmap ptr, no stack */

    /* Prefer 8x8 (CGA ROM font), fall back to 6x8 */
    fp = G_GetFont(0x0808);
    if (fp) { s_label_font = fp; s_label_font_w = 8; return; }
    fp = G_GetFont(0x0608);
    if (fp) { s_label_font = fp; s_label_font_w = 6; }
}

#define SET_LABEL_FONT() do { if (s_label_font) G_Font(s_label_font); } while(0)

/*---------------------------------------------------------------------------
 * Fill patterns for G_PATTERNFILL
 * Each 8-byte array is a repeating 8x8-pixel tile bitmask.
 *---------------------------------------------------------------------------*/

/* 50% hatch (alternating pixels) — used for LS_ABSENT (not in word) */
static unsigned char pat_hatch50[8] = {
    0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55
};

/* 25% light dot pattern — used for LS_MISPLACED (wrong position) */
static unsigned char pat_dot25[8] = {
    0x88, 0x00, 0x22, 0x00, 0x88, 0x00, 0x22, 0x00
};

/* Raw WORDLELX.ICN data (8-byte header + 44x32 bitmap payload). */
static unsigned char about_icon_raw[200] = {
    0x01, 0x00, 0x01, 0x00, 0x2C, 0x00, 0x20, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x0C, 0x00, 0x00, 0x00, 0x06, 0x80,
    0x3C, 0x00, 0x00, 0x00, 0x0F, 0xC0, 0x7F, 0x00,
    0x00, 0x00, 0x0F, 0xE0, 0x7F, 0x00, 0x00, 0x00,
    0x1F, 0xC0, 0x3F, 0x80, 0x00, 0x00, 0x3F, 0xF0,
    0x3F, 0x80, 0x00, 0x00, 0x3F, 0xE0, 0x3F, 0x80,
    0xD8, 0x00, 0x7F, 0xE0, 0x1F, 0xC0, 0xF8, 0x00,
    0xFF, 0xC0, 0x1F, 0xC1, 0xFE, 0x00, 0xFF, 0x80,
    0x0F, 0xC3, 0xFE, 0x01, 0xFF, 0x80, 0x0F, 0xE3,
    0xFF, 0x03, 0xFF, 0x00, 0x0F, 0xE7, 0xFF, 0x03,
    0xFE, 0x00, 0x07, 0xE7, 0xFF, 0x87, 0xFC, 0x00,
    0x07, 0xEF, 0xFF, 0x87, 0xFC, 0x00, 0x07, 0xFF,
    0xFF, 0xCF, 0xF8, 0x00, 0x07, 0xFF, 0xFF, 0xDF,
    0xF8, 0x00, 0x03, 0xFF, 0xFF, 0xFF, 0xF0, 0x00,
    0x03, 0xFF, 0xEF, 0x80, 0x00, 0x00, 0x03, 0xFF,
    0xE7, 0x8C, 0x18, 0xC0, 0x01, 0xFF, 0xC7, 0x98,
    0x31, 0x80, 0x01, 0xFF, 0xC3, 0x98, 0x1B, 0x00,
    0x01, 0xFF, 0x83, 0x98, 0x0E, 0x00, 0x00, 0xFF,
    0x81, 0x98, 0x0E, 0x00, 0x00, 0xFF, 0x01, 0xB0,
    0x36, 0x00, 0x00, 0x3E, 0x00, 0x30, 0x36, 0x00,
    0x00, 0x36, 0x00, 0x3E, 0x63, 0x00, 0x00, 0x00,
    0x00, 0x3E, 0x63, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

/*---------------------------------------------------------------------------
 * Internal helpers
 *---------------------------------------------------------------------------*/

static void draw_border(int x, int y, int w, int h, int border_w)
{
    int i;
    G_ColorSel(MAXCOLOR);
    G_RepRule(G_FORCE);
    for (i = 0; i < border_w; i++) {
        G_Move(x+i, y+i);
        G_Rect(x+w-1-i, y+h-1-i, G_OUTLINE);
    }
}

static void clear_rect_area(int x, int y, int w, int h)
{
    G_ColorSel(MINCOLOR);
    G_RepRule(G_FORCE);
    G_Move(x, y);
    G_Rect(x+w-1, y+h-1, G_SOLIDFILL);
}

/* Draw a letter into the tile using the cached 16x12 COUGRAPH font pointer.
   G_Font takes a raw bitmap ptr — no LHAPI font stack involved, no fallback. */
static void draw_tile_letter(int tx, int ty, char letter, int inverse)
{
    char buf[2];
    buf[0] = letter;
    buf[1] = '\0';
    if (s_tile_font) {
        G_Font(s_tile_font);
    } else {
        SET_LABEL_FONT();
    }
    if (inverse) {
        /* White-on-black: XOR black ink over the solid-black tile background */
        G_RepRule(G_XOR);
        G_ColorSel(MAXCOLOR);
        G_Text(tx, ty, buf, 0);
        G_RepRule(G_FORCE);
    } else {
        G_ColorSel(MAXCOLOR);
        G_RepRule(G_FORCE);
        G_Text(tx, ty, buf, 0);
    }
}

/* Draw a single tile at (x,y) with given letter and state.
   active: non-zero means this tile is in the current active input row. */
static void draw_tile(int x, int y, char letter, int state, int active)
{
    int tx = x + (TILE_W - FONT_LARGE_W) / 2;
    int ty = y + (TILE_H - FONT_LARGE_H) / 2;

    switch (state) {

    case LS_CORRECT:
        G_ColorSel(MAXCOLOR);
        G_RepRule(G_FORCE);
        G_Move(x, y);
        G_Rect(x+TILE_W-1, y+TILE_H-1, G_SOLIDFILL);
        if (letter)
            draw_tile_letter(tx, ty, letter, 1);
        break;

    case LS_MISPLACED:
        G_FillMask(pat_dot25);
        G_ColorSel(MAXCOLOR);
        G_RepRule(G_FORCE);
        G_Move(x, y);
        G_Rect(x+TILE_W-1, y+TILE_H-1, G_PATTERNFILL);
        G_Move(x, y);
        G_Rect(x+TILE_W-1, y+TILE_H-1, G_OUTLINE);
        if (letter)
            draw_tile_letter(tx, ty, letter, 0);
        break;

    case LS_ABSENT:
        G_FillMask(pat_hatch50);
        G_ColorSel(MAXCOLOR);
        G_RepRule(G_FORCE);
        G_Move(x, y);
        G_Rect(x+TILE_W-1, y+TILE_H-1, G_PATTERNFILL);
        G_Move(x, y);
        G_Rect(x+TILE_W-1, y+TILE_H-1, G_OUTLINE);
        if (letter)
            draw_tile_letter(tx, ty, letter, 0);
        break;

    default: /* LS_UNKNOWN — empty or active-input tile */
        clear_rect_area(x, y, TILE_W, TILE_H);
        if (active && letter) {
            draw_border(x, y, TILE_W, TILE_H, 2);
            draw_tile_letter(tx, ty, letter, 0);
        } else {
            G_ColorSel(MAXCOLOR);
            G_RepRule(G_FORCE);
            G_Move(x, y);
            G_Rect(x+TILE_W-1, y+TILE_H-1, G_OUTLINE);
        }
        break;
    }

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
        /* Light dot fill for wrong-position letters */
        G_FillMask(pat_dot25);
        G_ColorSel(MAXCOLOR);
        G_RepRule(G_FORCE);
        G_Move(x, y);
        G_Rect(x+KB_BOX_W-1, y+KB_BOX_H-1, G_PATTERNFILL);
        G_Move(x, y);
        G_Rect(x+KB_BOX_W-1, y+KB_BOX_H-1, G_OUTLINE);
        break;
    case LS_ABSENT:
        /* Dense hatch fill for letters not in word */
        G_FillMask(pat_hatch50);
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

/* Draw a compact legend indicator for help-dialog rows. */
static void draw_help_indicator(int x, int y, int state)
{
    const int bw = 14;
    const int bh = 8;

    switch (state) {
    case LS_CORRECT:
        G_ColorSel(MAXCOLOR);
        G_RepRule(G_FORCE);
        G_Move(x, y);
        G_Rect(x + bw - 1, y + bh - 1, G_SOLIDFILL);
        break;
    case LS_MISPLACED:
        G_FillMask(pat_dot25);
        G_ColorSel(MAXCOLOR);
        G_RepRule(G_FORCE);
        G_Move(x, y);
        G_Rect(x + bw - 1, y + bh - 1, G_PATTERNFILL);
        G_Move(x, y);
        G_Rect(x + bw - 1, y + bh - 1, G_OUTLINE);
        break;
    case LS_ABSENT:
        G_FillMask(pat_hatch50);
        G_ColorSel(MAXCOLOR);
        G_RepRule(G_FORCE);
        G_Move(x, y);
        G_Rect(x + bw - 1, y + bh - 1, G_PATTERNFILL);
        G_Move(x, y);
        G_Rect(x + bw - 1, y + bh - 1, G_OUTLINE);
        break;
    default:
        G_ColorSel(MAXCOLOR);
        G_RepRule(G_FORCE);
        G_Move(x, y);
        G_Rect(x + bw - 1, y + bh - 1, G_OUTLINE);
        break;
    }

    G_ColorSel(MAXCOLOR);
    G_RepRule(G_FORCE);
}

static void draw_about_icon(int x, int y, int scale)
{
    int row, col;
    int byte_index;
    unsigned char b;
    int px, py;

    for (row = 0; row < 32; row++) {
        for (col = 0; col < 44; col++) {
            byte_index = 8 + row * 6 + (col >> 3);
            b = about_icon_raw[byte_index];
            if ((b & (unsigned char)(0x80 >> (col & 7))) != 0) {
                px = x + col * scale;
                py = y + row * scale;
                G_Move(px, py);
                G_Rect(px + scale - 1, py + scale - 1, G_SOLIDFILL);
            }
        }
    }
}

/*---------------------------------------------------------------------------
 * Centering helpers (use s_label_font_w to get the right x regardless of
 * which font the system has)
 *---------------------------------------------------------------------------*/
static int center_x_in_panel(int num_chars)
{
    return KBOARD_X + (RIGHT_PANEL_W - num_chars * s_label_font_w) / 2;
}

static int center_x_for_msg(int num_chars)
{
    int tx = KBOARD_X + (RIGHT_PANEL_W - num_chars * s_label_font_w) / 2;
    if (tx < KBOARD_X) tx = KBOARD_X;
    return tx;
}

/* Center x for fixed-width label font text in an arbitrary rectangle. */
static int center_x_in_rect(int left, int width, int num_chars)
{
    return left + (width - num_chars * s_label_font_w) / 2;
}

/*---------------------------------------------------------------------------
 * Public drawing functions
 *---------------------------------------------------------------------------*/

void exm_draw_chrome(void)
{
    /*
     * Title "Wordle LX" centered above the game board:
     *   x = BOARD_X + (BOARD_AREA_W - 9*font_w) / 2
     *
     * Right panel centering uses s_label_font_w so positions auto-correct
     * whether the system has a 6- or 8-pixel-wide default font.
     *
     * Legend block width = KB_BOX_W + 3 + widest_label_chars * font_w
     *   widest label = "[/]=Not in word" = 15 chars → 15 * font_w
     *   total = 22 + 3 + 15 * font_w = 25 + 15 * font_w
     */
    int kb_row_h = KB_BOX_H + KB_GAP + 2;       /* height of one keyboard row step */
    int kb_bottom = KBOARD_Y + 3 * kb_row_h;    /* first pixel below all 3 rows    */
    int lgnd_y1   = kb_bottom + 6;
    int lgnd_y2   = lgnd_y1 + 16;
    int lgnd_y3   = lgnd_y2 + 16;
    int lgnd_block_w = KB_BOX_W + 3 + 15 * s_label_font_w;
    int lgnd_x    = KBOARD_X + (RIGHT_PANEL_W - lgnd_block_w) / 2;
    int lgnd_tx   = lgnd_x + KB_BOX_W + 3;
    int label_x   = center_x_in_panel(17); /* "Remaining Letters" = 17 chars */

    SET_LABEL_FONT();
    G_ColorSel(MAXCOLOR);
    G_RepRule(G_FORCE);

    /* Vertical divider below the top title bar */
    G_Move(DIVIDER_X, TITLE_BAR_H);
    G_Draw(DIVIDER_X, 189);

    /* "Remaining Letters" label at top of right panel */
    G_Text(label_x, RIGHT_LABEL_Y, szLabel, 0);

    /* Separator below label+keyboard block, above legend */
    G_Move(KBOARD_X, kb_bottom + 3);
    G_Draw(630, kb_bottom + 3);

    /* Legend: indicator box + label, centered as a group */
    draw_kb_indicator(lgnd_x, lgnd_y1, LS_CORRECT);
    SET_LABEL_FONT();
    G_Text(lgnd_tx, lgnd_y1 + 3, szLegend1, 0);

    draw_kb_indicator(lgnd_x, lgnd_y2, LS_MISPLACED);
    SET_LABEL_FONT();
    G_Text(lgnd_tx, lgnd_y2 + 3, szLegend2, 0);

    draw_kb_indicator(lgnd_x, lgnd_y3, LS_ABSENT);
    SET_LABEL_FONT();
    G_Text(lgnd_tx, lgnd_y3 + 3, szLegend3, 0);

    /* Separator above message area */
    G_Move(KBOARD_X, RIGHT_MSG_Y - 3);
    G_Draw(630, RIGHT_MSG_Y - 3);
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
                draw_tile(x, y,
                          gs->rows[row].letters[col],
                          (int)gs->rows[row].result[col],
                          0);
            } else if (is_active_row) {
                c = (col < gs->input_len) ? gs->input[col] : '\0';
                draw_tile(x, y, c, LS_UNKNOWN, 1);
            } else {
                draw_tile(x, y, '\0', LS_UNKNOWN, 0);
            }
        }
    }
}

void exm_draw_keyboard(const GameState far *gs)
{
    static const char row1[] = "QWERTYUIOP";
    static const char row2[] = "ASDFGHJKL";
    static const char row3[] = "ZXCVBNM";

    int panel_w = 630 - KBOARD_X;
    int box_step = KB_BOX_W + KB_GAP;

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
        tx = x1_base + i * box_step + (KB_BOX_W - s_label_font_w) / 2;
        ty = y1 + (KB_BOX_H - 8) / 2;
        SET_LABEL_FONT();
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
        tx = x2_base + i * box_step + (KB_BOX_W - s_label_font_w) / 2;
        ty = y2 + (KB_BOX_H - 8) / 2;
        SET_LABEL_FONT();
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
        tx = x3_base + i * box_step + (KB_BOX_W - s_label_font_w) / 2;
        ty = y3 + (KB_BOX_H - 8) / 2;
        SET_LABEL_FONT();
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
    clear_rect_area(KBOARD_X, RIGHT_MSG_Y, RIGHT_PANEL_W, 30);

    if (msg) {
        const char far *p = msg;
        int len = 0;
        int tx;
        while (*p++) len++;
        tx = center_x_for_msg(len);
        SET_LABEL_FONT();
        G_ColorSel(MAXCOLOR);
        G_RepRule(G_FORCE);
        G_Text(tx, RIGHT_MSG_Y + 1, (char far *)msg, 0);
    }
}

void exm_draw_message2(const char far *line1, const char far *line2)
{
    const char far *p;
    int len, tx;

    clear_rect_area(KBOARD_X, RIGHT_MSG_Y, RIGHT_PANEL_W, 30);

    SET_LABEL_FONT();
    G_ColorSel(MAXCOLOR);
    G_RepRule(G_FORCE);

    if (line1) {
        p = line1; len = 0;
        while (*p++) len++;
        tx = center_x_for_msg(len);
        G_Text(tx, RIGHT_MSG_Y + 1, (char far *)line1, 0);
    }
    if (line2) {
        p = line2; len = 0;
        while (*p++) len++;
        tx = center_x_for_msg(len);
        G_Text(tx, RIGHT_MSG_Y + 11, (char far *)line2, 0);
    }
}

void exm_draw_message3(const char far *line1, const char far *line2, const char far *line3)
{
    const char far *p;
    int len, tx;

    clear_rect_area(KBOARD_X, RIGHT_MSG_Y, RIGHT_PANEL_W, 32);

    SET_LABEL_FONT();
    G_ColorSel(MAXCOLOR);
    G_RepRule(G_FORCE);

    if (line1) {
        p = line1; len = 0;
        while (*p++) len++;
        tx = center_x_for_msg(len);
        G_Text(tx, RIGHT_MSG_Y + 1, (char far *)line1, 0);
    }
    if (line2) {
        p = line2; len = 0;
        while (*p++) len++;
        tx = center_x_for_msg(len);
        G_Text(tx, RIGHT_MSG_Y + 11, (char far *)line2, 0);
    }
    if (line3) {
        p = line3; len = 0;
        while (*p++) len++;
        tx = center_x_for_msg(len);
        G_Text(tx, RIGHT_MSG_Y + 21, (char far *)line3, 0);
    }
}

void exm_draw_help_dialog(void)
{
    static char h_title[] = "WORDLE LX HELP";
    static char h_l1[]  = "Goal: guess the hidden five-letter word in six tries.";
    static char h_l2[]  = "Type letters A-Z to fill the active row. Backspace deletes.";
    static char h_l3[]  = "Press Enter to submit a guess when five letters are filled.";
    static char h_l4[]  = "Each submitted tile shows feedback:";
    static char h_l5[]  = "";
    static char h_l6[]  = "= correct letter in the correct position.";
    static char h_l7[]  = "= letter is in the word, wrong position.";
    static char h_l8[]  = "= letter is not in the answer.";
    static char h_l9[]  = "";
    static char h_l10[] = "The right panel keyboard keeps best-known status per letter.";
    static char h_l11[] = "Use this to avoid repeats and narrow candidates efficiently.";
    static char h_l12[] = "A valid dictionary word is required for each submitted guess.";
    static char h_l13[] = "";
    static char h_l14[] = "Tip: prioritize coverage early, then lock positions quickly.";
    static char *lines[] = {
        h_l1, h_l2, h_l3, h_l4, h_l5, h_l6, h_l7, h_l8,
        h_l9, h_l10, h_l11, h_l12, h_l13, h_l14
    };
    int x, y, w, h;
    int i, len, tx;
    int header_h;
    int body_y;
    int legend_x;
    int legend_tx;
    int legend_y;
    char *p;

    x = 20;
    y = 14;
    w = 600;
    h = 168;
    header_h = 12;
    body_y = y + header_h + 4;
    legend_x = x + 120;
    legend_tx = legend_x + 20;

    clear_rect_area(x, y, w, h);
    draw_border(x, y, w, h, 2);

    G_ColorSel(MAXCOLOR);
    G_RepRule(G_FORCE);
    G_Move(x + 1, y + header_h);
    G_Draw(x + w - 2, y + header_h);

    SET_LABEL_FONT();

    p = h_title;
    len = 0;
    while (*p++) len++;
    tx = center_x_in_rect(x, w, len);
    G_Text(tx, y + 2, h_title, 0);

    for (i = 0; i < (int)(sizeof(lines) / sizeof(lines[0])); i++) {
        if (i >= 5 && i <= 7) {
            legend_y = body_y + i * 9;
            if (i == 5) draw_help_indicator(legend_x, legend_y, LS_CORRECT);
            if (i == 6) draw_help_indicator(legend_x, legend_y, LS_MISPLACED);
            if (i == 7) draw_help_indicator(legend_x, legend_y, LS_ABSENT);
            G_Text(legend_tx, legend_y, lines[i], 0);
            continue;
        }
        p = lines[i];
        len = 0;
        while (*p++) len++;
        tx = center_x_in_rect(x + 8, w - 16, len);
        if (tx < x + 8) tx = x + 8;
        G_Text(tx, body_y + i * 9, lines[i], 0);
    }
}

void exm_draw_about_dialog(void)
{
    static char a_l1[] = "Programmed by Jarrod Kozeal";
    static char a_l2[] = "Using Claude Code - 2026";
    static char a_l3[] = "";
    static char a_l4[] = "https://github.com/jr0dsgarage/wordlelx";

    int x, y, w, h;
    int icon_scale;
    int icon_w;
    int icon_h;
    int icon_x;
    int icon_y;
    int icon_box_x;
    int icon_box_y;
    int icon_box_w;
    int icon_box_h;
    int text_x;
    int text_y;

    x = 48;
    y = 26;
    w = 544;
    h = 124;

    icon_scale = 2;
    icon_w = 44 * icon_scale;
    icon_h = 32 * icon_scale;

    icon_box_x = x + 16;
    icon_box_y = y + 12;
    icon_box_w = icon_w + 16;
    icon_box_h = icon_h + 16;
    icon_x = icon_box_x + 8;
    icon_y = icon_box_y + 8;

    text_x = icon_box_x + icon_box_w + 20;
    text_y = y + 24;

    clear_rect_area(x, y, w, h);
    draw_border(x, y, w, h, 2);

    SET_LABEL_FONT();

    draw_border(icon_box_x, icon_box_y, icon_box_w, icon_box_h, 1);
    draw_about_icon(icon_x, icon_y, icon_scale);

    G_Text(text_x, text_y + 0, a_l1, 0);
    G_Text(text_x, text_y + 10, a_l2, 0);
    G_Text(text_x, text_y + 20, a_l3, 0);
    G_Text(text_x, text_y + 30, a_l4, 0);
}

void exm_draw_active_row(const GameState far *gs)
{
    int col, x, y;
    char c;
    if (gs->over) return;
    y = BOARD_Y + gs->num_guesses * (TILE_H + TILE_VG);
    for (col = 0; col < WORD_LEN; col++) {
        x = BOARD_X + col * (TILE_W + TILE_HG);
        c = (col < gs->input_len) ? gs->input[col] : '\0';
        draw_tile(x, y, c, LS_UNKNOWN, 1);
    }
}
