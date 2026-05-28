#ifndef GUESSES_H
#define GUESSES_H

#include <string.h>

/* Returns 1 if word is a valid Wordle guess (not necessarily an answer), 0 otherwise. */
int guesses_is_valid(const char *word);

#endif
