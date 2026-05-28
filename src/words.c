#include <memory.h>
#include "words.h"
#include "worddata.h"

static char word_buf[WORD_LEN + 1];

int words_count(void) { return worddata_answer_count; }

const char *words_get(int i)
{
    if (i < 0 || i >= worddata_answer_count || !worddata_answers) return 0;
    _fmemcpy(word_buf, worddata_answers + (unsigned)i * WORD_LEN, WORD_LEN);
    word_buf[WORD_LEN] = '\0';
    return word_buf;
}

int words_contains(const char *word)
{
    int lo = 0, hi = worddata_answer_count - 1, mid, cmp;
    while (lo <= hi) {
        mid = (lo + hi) / 2;
        cmp = _fmemcmp(word, worddata_answers + (unsigned)mid * WORD_LEN, WORD_LEN);
        if (cmp == 0) return 1;
        if (cmp < 0)  hi = mid - 1;
        else          lo = mid + 1;
    }
    return 0;
}
