#ifndef GAME_H
#define GAME_H

#define WORD_LEN   5
#define MAX_GUESSES 6

typedef enum {
    LS_UNKNOWN  = 0,
    LS_ABSENT   = 1,
    LS_MISPLACED = 2,
    LS_CORRECT  = 3
} LetterState;

typedef struct {
    char        letters[WORD_LEN + 1];
    LetterState result[WORD_LEN];
} GuessRow;

typedef struct {
    char      answer[WORD_LEN + 1];
    GuessRow  rows[MAX_GUESSES];
    int       num_guesses;
    int       keyboard[26];   /* best LetterState per A-Z */
    int       won;
    int       over;
    /* current partial input — maintained by the game loop, read by display */
    char      input[WORD_LEN + 1];
    int       input_len;
} GameState;

void game_init(GameState *gs, const char *answer);
void game_score_guess(GameState *gs, const char *guess);

#endif
