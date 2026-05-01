#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

#ifdef __WATCOMC__
#include <conio.h>
#else
#include <termios.h>
#include <unistd.h>
/* raw-mode getch for Mac host testing */
static int getch(void)
{
    struct termios oldt, newt;
    int c;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    c = getchar();
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    return c;
}
#endif

#include "game.h"
#include "display.h"
#include "words.h"
#include "guesses.h"
#include "worddata.h"

/* ---- Main game loop ----
 * Returns 1 when the game ends naturally (won or lost).
 * Returns 0 if the player pressed ESC to quit.             */
static int run_game(GameState *gs, Display *d)
{
    int key;

    while (!gs->over) {
        key = getch();

        if (key == 27) {                         /* ESC — quit */
            return 0;

        } else if (key == '\r' || key == '\n') { /* Enter — submit */
            if (gs->input_len == WORD_LEN) {
                if (!words_contains(gs->input) && !guesses_is_valid(gs->input)) {
                    char msg[40];
                    sprintf(msg, "\"%s\" is not a valid guess!", gs->input);
                    gs->input_len = 0;
                    memset(gs->input, 0, sizeof(gs->input));
                    d->draw_board(gs);
                    d->draw_message(msg);
                } else {
                    game_score_guess(gs, gs->input);
                    gs->input_len = 0;
                    memset(gs->input, 0, sizeof(gs->input));
                    d->draw_board(gs);
                    d->draw_keyboard(gs);
                }
            } else {
                d->draw_message("Need 5 letters");
            }

        } else if (key == '\b' || key == 127) {  /* Backspace */
            if (gs->input_len > 0) {
                gs->input_len--;
                gs->input[gs->input_len] = '\0';
            }
            d->draw_board(gs);
            d->draw_message(NULL);

        } else if (isalpha(key) && gs->input_len < WORD_LEN) {
            gs->input[gs->input_len++] = (char)toupper(key);
            gs->input[gs->input_len]   = '\0';
            d->draw_board(gs);
            d->draw_message(NULL);
        }
    }
    return 1; /* game over naturally */
}

/* ---- Entry point ---- */

int main(int argc, char *argv[])
{
    Display   disp;
    GameState gs;
    int       word_index;
    const char *answer;

    if (!worddata_load_auto(argc > 0 ? argv[0] : 0)) {
        fprintf(stderr, "Cannot find WORDLELX.DAT\n");
        return 1;
    }

    srand((unsigned int)time(NULL));
    text_mode_init_display(&disp);
    disp.init();

    for (;;) {
        char msg[48];
        int  key;

        word_index = rand() % words_count();
        answer     = words_get(word_index);
        game_init(&gs, answer);

        disp.draw_board(&gs);
        disp.draw_keyboard(&gs);

        if (!run_game(&gs, &disp)) break;   /* ESC pressed */

        /* Game over: show result and play-again prompt */
        if (gs.won) {
            disp.draw_message("You won!   Play again?  [Y] / [N]");
        } else {
            sprintf(msg, "Answer: %s   Play again?  [Y] / [N]", gs.answer);
            disp.draw_message(msg);
        }

        key = toupper(getch());
        if (key != 'Y') break;

        disp.init();  /* clear and redraw chrome for the new game */
    }

    disp.cleanup();
    return 0;
}
