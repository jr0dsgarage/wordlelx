#include <memory.h>
#include "guesses.h"
#include "worddata.h"

int guesses_is_valid(const char *word)
{
    unsigned lo = 0, hi, mid;
    int cmp;
    if (worddata_guess_count == 0 || !worddata_guesses) return 0;
    hi = (unsigned)worddata_guess_count - 1;
    while (lo <= hi) {
        mid = (lo + hi) / 2;
        cmp = _fmemcmp(word, worddata_guesses + mid * 5, 5);
        if (cmp == 0) return 1;
        if (cmp < 0)  hi = mid - 1;
        else          lo = mid + 1;
    }
    return 0;
}
