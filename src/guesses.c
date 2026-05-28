#include <memory.h>
#include "guesses.h"
#include "worddata.h"

int guesses_is_valid(const char *word)
{
    return worddata_guess_contains(word);
}
