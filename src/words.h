#ifndef WORDS_H
#define WORDS_H

#include "game.h"

int         words_count(void);
const char *words_get(int index);
int         words_contains(const char *word); /* 1 if word is a valid answer */

#endif
