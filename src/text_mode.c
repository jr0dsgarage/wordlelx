/*
 * text_mode.c — HP 200LX split-screen Wordle display
 *
 * 80×25 CGA text mode, greyscale-friendly.
 *
 * Col  0-38 : game board (6 guess rows)
 * Col 39    : vertical divider │
 * Col 40-79 : alphabet status + legend + instructions
 * Row 24    : credit line across full width
 *
 * Tile visual scheme (3 chars wide, used on BOTH halves):
 *   CORRECT    A   attr 0x70  inverted: light bg, dark letter
 *   MISPLACED  A   attr 0x0F  dark bg, bright-white letter
 *   ABSENT     A   attr 0x08  dark bg, dim letter
 *   UNKNOWN    A   attr 0x07  dark bg, grey — not yet tried
 *   (active)  [A]  attr 0x0F  brackets mark the active input row (typed)
 *   (active)  [_]  attr 0x07  brackets mark the active input row (empty)
 *
 * All scored/keyboard tiles are " A " (space-letter-space); differentiation
 * is by attribute only.  Active-row tiles keep brackets to visually mark
 * the row currently being typed.
 */

#include <string.h>
#include "game.h"
#include "display.h"

#ifdef __WATCOMC__
#include <i86.h>
#include <conio.h>
#define VRAM ((unsigned char far *)0xB8000000UL)
#else
#include <stdio.h>
#define VRAM ((unsigned char *)0)
static void int86(int n, void *r, void *o) { (void)n;(void)r;(void)o; }
#endif

/* ── Layout constants ─────────────────────────────────────────── */

#define LH_END     38          /* last column of left half          */
#define DIVIDER    39          /* column of the │ divider           */
#define RH_START   40          /* first column of right half        */
#define RH_WIDTH   40          /* right half width                  */

/* Board (left half) */
#define TILE_W      3          /* chars per tile                        */
#define BOARD_COLS (TILE_W * WORD_LEN)          /* 15 chars             */
#define BOARD_LEFT ((LH_END + 1 - BOARD_COLS) / 2)   /* = 12           */
#define BOARD_TOP   4          /* first guess row (blank row 3 above)   */
#define BOARD_STEP  2          /* screen rows per guess slot            */

/*
 * Row map (all full-width ─ rows skip the │ divider):
 *   0  : full-width top border ─────────────────────────────────
 *   1  : WORDLE LX (left)        LETTERS USED (right)
 *   2  : full-width under-title ──────────────────────────────── (LH_SEP1)
 *   3  : blank                   blank
 *   4  : Guess 1                 QWERTYUIOP
 *   6  : Guess 2                 ASDFGHJKL
 *   8  : Guess 3                 ZXCVBNM
 *  10  : Guess 4                 ── right-half separator ────── (RH_SEP1)
 *  11  :                         LEGEND:
 *  12  : Guess 5                  X  Correct position (inverted)
 *  13  :                          X  In word, wrong position (bright)
 *  14  : Guess 6                  X  Not in word (dim)
 *  15  :                          _  Not tried (normal)
 *  16  : full-width below-content separator ─────────────────── (LH_SEP2)
 *  17  : message / feedback      blank
 *  18  :                         ENTER  submit guess
 *  19  :                         BKSP   delete letter
 *  20  :                         ESC    quit game
 *  24  : credit line (full width)
 */
#define LH_SEP1     2          /* full-width under-title separator      */
#define LH_SEP2    16          /* full-width below-content separator    */
#define MSG_ROW    17          /* message / feedback row                */
#define CREDIT_ROW 24

/* Right half keyboard rows */
#define RH_ALPHA1   4
#define RH_ALPHA2   6
#define RH_ALPHA3   8

/* Right half panel rows */
#define RH_SEP1    10          /* interior separator (right half only)  */
#define RH_LEGEND  11

/* Right half instruction rows (bottom section) */
#define RH_INSTR1  18
#define RH_INSTR2  19
#define RH_INSTR3  20

/* ── VRAM helpers ─────────────────────────────────────────────── */

static void vram_put(int col, int row, unsigned char ch, unsigned char attr)
{
#ifdef __WATCOMC__
    unsigned int off = (unsigned int)(row * 80 + col) * 2;
    VRAM[off]     = ch;
    VRAM[off + 1] = attr;
#else
    (void)col; (void)row; (void)ch; (void)attr;
#endif
}

static void vram_str(int col, int row, const char *s, unsigned char attr)
{
    while (*s) vram_put(col++, row, (unsigned char)*s++, attr);
}

static void vram_fill(int col, int row, int width, unsigned char ch, unsigned char attr)
{
    int i;
    for (i = 0; i < width; i++) vram_put(col + i, row, ch, attr);
}

static void clear_screen(void)
{
    int r;
    for (r = 0; r < 25; r++)
        vram_fill(0, r, 80, ' ', 0x07);
}

static void set_text_mode(void)
{
#ifdef __WATCOMC__
    union REGS r;
    r.h.ah = 0x00;
    r.h.al = 0x03;
    int86(0x10, &r, &r);
#endif
}

static void hide_cursor(void)
{
#ifdef __WATCOMC__
    union REGS r;
    r.h.ah = 0x01;
    r.h.ch = 0x20;
    r.h.cl = 0x00;
    int86(0x10, &r, &r);
#endif
}

static void show_cursor(void)
{
#ifdef __WATCOMC__
    union REGS r;
    r.h.ah = 0x01;
    r.h.ch = 0x06;
    r.h.cl = 0x07;
    int86(0x10, &r, &r);
#endif
}

/* ── Tile drawing ─────────────────────────────────────────────── */

/*
 * Draw a 3-char tile at (col, row) for a scored or keyboard letter.
 * For LS_UNKNOWN, pass letter='A'-'Z' for keyboard use,
 * or letter=0 to render [_] (empty board slot).
 */
static void draw_tile(int col, int row, char letter, int state)
{
    unsigned char lc, rc, attr;

    switch (state) {
        case LS_CORRECT:
            lc = ' '; rc = ' '; attr = 0x70; break;
        case LS_MISPLACED:
            lc = ' '; rc = ' '; attr = 0x0F; break;
        case LS_ABSENT:
            lc = ' '; rc = ' '; attr = 0x08; break;
        default: /* LS_UNKNOWN */
            lc = ' '; rc = ' '; attr = 0x07;
            if (!letter) letter = '_';
            break;
    }

    vram_put(col,     row, lc,                    attr);
    vram_put(col + 1, row, (unsigned char)letter,  attr);
    vram_put(col + 2, row, rc,                    attr);
}

/* Active row: typed letter = bright [A], empty slot = grey [_] */
static void draw_active_tile(int col, int row, char letter)
{
    if (letter) {
        vram_put(col,     row, '[',                    0x0F);
        vram_put(col + 1, row, (unsigned char)letter,  0x0F);
        vram_put(col + 2, row, ']',                    0x0F);
    } else {
        vram_put(col,     row, '[', 0x07);
        vram_put(col + 1, row, '_', 0x07);
        vram_put(col + 2, row, ']', 0x07);
    }
}

/* Future (not-yet-reached) row: dim placeholder dot */
static void draw_future_tile(int col, int row)
{
    vram_put(col,     row, ' ', 0x08);
    vram_put(col + 1, row, '.', 0x08);
    vram_put(col + 2, row, ' ', 0x08);
}

/* ── Board (left half) ────────────────────────────────────────── */

static void tm_draw_board(const GameState *gs)
{
    int r, c, tile_row, tile_col;
    char letter;
    int  state;

    for (r = 0; r < MAX_GUESSES; r++) {
        tile_row = BOARD_TOP + r * BOARD_STEP;

        for (c = 0; c < WORD_LEN; c++) {
            tile_col = BOARD_LEFT + c * TILE_W;

            if (r < gs->num_guesses) {
                letter = gs->rows[r].letters[c];
                state  = gs->rows[r].result[c];
                draw_tile(tile_col, tile_row, letter, state);
            } else if (r == gs->num_guesses && !gs->over) {
                letter = (c < gs->input_len) ? gs->input[c] : '\0';
                draw_active_tile(tile_col, tile_row, letter);
            } else {
                draw_future_tile(tile_col, tile_row);
            }
        }
    }
}

/* ── Keyboard status (right half) ────────────────────────────── */

/*
 * Three rows in QWERTY keyboard order, each letter shown as a tile
 * using the same visual scheme as the board.
 */
static void tm_draw_keyboard(const GameState *gs)
{
    static const char *alpha_rows[3] = { "QWERTYUIOP", "ASDFGHJKL", "ZXCVBNM" };
    static const int   alpha_lens[3] = { 10, 9, 7 };
    static const int   screen_rows[3] = { RH_ALPHA1, RH_ALPHA2, RH_ALPHA3 };

    /* All vars declared at function scope — C89 requires no decl after stmt */
    int r, c, count, row_width, start_col, scrow;
    char letter;

    for (r = 0; r < 3; r++) {
        count     = alpha_lens[r];
        scrow     = screen_rows[r];
        /* Each slot = tile(3) + space(1), last tile has no trailing space.
         * Total row width = count*3 + (count-1)*1 = 4*count - 1         */
        row_width = count * 4 - 1;
        start_col = RH_START + (RH_WIDTH - row_width) / 2;

        vram_fill(RH_START, scrow, RH_WIDTH, ' ', 0x07);

        for (c = 0; c < count; c++) {
            letter = alpha_rows[r][c];
            draw_tile(start_col + c * 4, scrow, letter,
                      gs->keyboard[letter - 'A']);
            if (c < count - 1)
                vram_put(start_col + c * 4 + TILE_W, scrow, ' ', 0x07);
        }
    }
}

/* ── Message (left half, MSG_ROW) ────────────────────────────── */

static void tm_draw_message(const char *msg)
{
    vram_fill(0, MSG_ROW, LH_END + 1, ' ', 0x07);
    if (msg) {
        int start = (LH_END + 1 - (int)strlen(msg)) / 2;
        if (start < 0) start = 0;
        vram_str(start, MSG_ROW, msg, 0x0F);
    }
}

/* ── Static chrome (drawn once at init) ──────────────────────── */

static void draw_chrome(void)
{
    static const unsigned char leg_mc[4] = { 'X', 'X', 'X', '_' };
    static const unsigned char leg_at[4] = { 0x70, 0x0F, 0x08, 0x07 };
    static const char * const  leg_ds[4] = { " Correct position",
                                              " In word, wrong position",
                                              " Not in word",
                                              " Not tried" };
    int r, i, lr;

    /* ── Top border (full width, row 0) ────────────────── */

    vram_fill(0, 0, 80, 0xC4, 0x07);

    /* ── Left half ─────────────────────────────────────── */

    vram_str((LH_END + 1 - 10) / 2, 1, "WORDLE  LX", 0x0F);

    /* Under-title separator: full width at LH_SEP1 */
    vram_fill(0, LH_SEP1, 80, 0xC4, 0x07);

    /* ── Divider (skip top border, under-title, and bottom separator) ── */

    for (r = 0; r < CREDIT_ROW; r++) {
        if (r != 0 && r != LH_SEP1 && r != LH_SEP2)
            vram_put(DIVIDER, r, 0xB3 /* │ */, 0x07);
    }

    /* ── Right half ────────────────────────────────────── */

    vram_str(RH_START + (RH_WIDTH - 12) / 2, 1, "LETTERS USED", 0x07);

    vram_fill(RH_START, RH_SEP1, 40, 0xC4, 0x07);

    /* Legend */
    vram_str(RH_START + 1, RH_LEGEND, "LEGEND:", 0x07);
    for (i = 0; i < 4; i++) {
        lr = RH_LEGEND + 1 + i;
        vram_put(RH_START + 2, lr, ' ',       leg_at[i]);
        vram_put(RH_START + 3, lr, leg_mc[i], leg_at[i]);
        vram_put(RH_START + 4, lr, ' ',       leg_at[i]);
        vram_str(RH_START + 5, lr, leg_ds[i], 0x07);
    }

    /* Bottom separator: full-width across both halves */
    vram_fill(0, LH_SEP2, 80, 0xC4, 0x07);

    /* Instructions */
    vram_str(RH_START + 2, RH_INSTR1, "ENTER  submit guess",  0x07);
    vram_str(RH_START + 2, RH_INSTR2, "BKSP   delete letter", 0x07);
    vram_str(RH_START + 2, RH_INSTR3, "ESC    quit game",     0x07);

    /* ── Credit line (full width, row 24) ─────────────── */
    /* 52 chars centred in 80 */
    vram_str((80 - 52) / 2, CREDIT_ROW,
             "Programmed by Jarrod Kozeal using Claude Code - 2026", 0x08);
}

/* ── Display interface ────────────────────────────────────────── */

static void tm_init(void)
{
    set_text_mode();
    hide_cursor();
    clear_screen();
    draw_chrome();
}

static void tm_cleanup(void)
{
    show_cursor();
}

void text_mode_init_display(Display *d)
{
    d->init          = tm_init;
    d->draw_board    = tm_draw_board;
    d->draw_keyboard = tm_draw_keyboard;
    d->draw_message  = tm_draw_message;
    d->cleanup       = tm_cleanup;
}
