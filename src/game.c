#include <string.h>
#include "game.h"

void game_init(GameState *gs, const char *answer)
{
    int i;
    memset(gs, 0, sizeof(*gs));
    for (i = 0; i < WORD_LEN; i++)
        gs->answer[i] = answer[i];
    gs->answer[WORD_LEN] = '\0';
}

/*
 * Standard Wordle scoring: correct positions first, then misplaced.
 * Handles duplicate letters correctly — each answer letter is only
 * "consumed" once.
 */
void game_score_guess(GameState *gs, const char *guess)
{
    GuessRow *row;
    int answer_used[WORD_LEN];
    int i, j;

    if (gs->over)
        return;

    row = &gs->rows[gs->num_guesses];
    memcpy(row->letters, guess, WORD_LEN);
    row->letters[WORD_LEN] = '\0';

    for (i = 0; i < WORD_LEN; i++) {
        row->result[i] = LS_ABSENT;
        answer_used[i] = 0;
    }

    /* Pass 1: mark correct positions */
    for (i = 0; i < WORD_LEN; i++) {
        if (row->letters[i] == gs->answer[i]) {
            row->result[i] = LS_CORRECT;
            answer_used[i] = 1;
        }
    }

    /* Pass 2: mark misplaced */
    for (i = 0; i < WORD_LEN; i++) {
        if (row->result[i] == LS_CORRECT)
            continue;
        for (j = 0; j < WORD_LEN; j++) {
            if (!answer_used[j] && row->letters[i] == gs->answer[j]) {
                row->result[i] = LS_MISPLACED;
                answer_used[j] = 1;
                break;
            }
        }
    }

    /* Update per-letter keyboard state (keep best result seen) */
    for (i = 0; i < WORD_LEN; i++) {
        int idx = row->letters[i] - 'A';
        if (idx >= 0 && idx < 26) {
            if (row->result[i] > gs->keyboard[idx])
                gs->keyboard[idx] = row->result[i];
        }
    }

    gs->num_guesses++;

    if (memcmp(row->letters, gs->answer, WORD_LEN) == 0) {
        gs->won  = 1;
        gs->over = 1;
    } else if (gs->num_guesses >= MAX_GUESSES) {
        gs->over = 1;
    }
}
