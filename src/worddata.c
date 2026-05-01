/* worddata.c — loads WORDLELX.DAT into memory-efficient structures.
 *
 * DAT format (magic "WRD5"):
 *   [0-3]  magic "WRD5"
 *   [4-5]  answer_count  (uint16 LE)
 *   [6-7]  guess_count   (uint16 LE)
 *   [8..]  answer words  packed: 5 bits/letter, MSB first, padded to byte boundary
 *   [...]  guess words   packed: same encoding, independent byte boundary
 */

#include <stdio.h>
#include <malloc.h>
#include <memory.h>
#include <string.h>
#include "worddata.h"

#define DAT_NAME  "WORDLELX.DAT"

char __far *worddata_answers      = 0;
int         worddata_answer_count = 0;
char __far *worddata_guesses      = 0;  /* unused in packed mode */
int         worddata_guess_count  = 0;

static char s_dat_path[128] = "";
static long s_guess_ofs = 0L;

/* --- Bit-stream reader --------------------------------------------------- */

typedef struct {
    FILE         *f;
    unsigned long accum;
    int           bits;
} BitReader;

static void br_init(BitReader *br, FILE *f)
{
    br->f = f;
    br->accum = 0UL;
    br->bits = 0;
}

static int br_read5(BitReader *br)
{
    unsigned char b;
    while (br->bits < 5) {
        if (fread(&b, 1, 1, br->f) != 1) return -1;
        br->accum = (br->accum << 8) | (unsigned long)b;
        br->bits += 8;
    }
    br->bits -= 5;
    return (int)((br->accum >> br->bits) & 0x1FUL);
}

static int unpack_words(BitReader *br, char __far *dst, int count)
{
    int i, v;
    for (i = 0; i < count * 5; i++) {
        v = br_read5(br);
        if (v < 0) return 0;
        *dst++ = (char)('A' + v);
    }
    return 1;
}

/* --- Path helper --------------------------------------------------------- */

int worddata_load_sibling(const char *exe_path)
{
    char        path[128];
    char       *d = path;
    const char *p, *sep, *name;

    name = DAT_NAME;
    if (exe_path) {
        sep = 0;
        for (p = exe_path; *p; p++)
            if (*p == '\\' || *p == '/') sep = p;
        if (sep) {
            for (p = exe_path; p <= sep; ) *d++ = *p++;
        }
    }
    for (p = name; *p; ) *d++ = *p++;
    *d = '\0';
    return worddata_load(path);
}

/* --- Packed guess lookup ------------------------------------------------- */

static int read_packed_word_at(FILE *f, long base_ofs, unsigned index, char out[5])
{
    unsigned long bit_ofs;
    long          byte_ofs;
    int           bit_shift;
    unsigned char b[4];
    unsigned long accum;
    int           i;

    bit_ofs   = (unsigned long)index * 25UL;
    byte_ofs  = base_ofs + (long)(bit_ofs >> 3);
    bit_shift = (int)(bit_ofs & 7UL);

    if (fseek(f, byte_ofs, SEEK_SET) != 0)
        return 0;
    if (fread(b, 1, 4, f) != 4)
        return 0;

    accum = ((unsigned long)b[0] << 24) |
            ((unsigned long)b[1] << 16) |
            ((unsigned long)b[2] << 8)  |
            ((unsigned long)b[3]);

    for (i = 0; i < 5; i++) {
        int shift = 32 - (bit_shift + (i + 1) * 5);
        int val = (int)((accum >> shift) & 0x1FUL);
        out[i] = (char)('A' + val);
    }
    return 1;
}

int worddata_guess_contains(const char *word)
{
    FILE     *f;
    unsigned  lo, hi, mid;
    char      mid_word[5];
    int       cmp;

    if (worddata_guess_count <= 0 || !word || !s_dat_path[0])
        return 0;

    lo = 0;
    hi = (unsigned)worddata_guess_count - 1;

    f = fopen(s_dat_path, "rb");
    if (!f)
        return 0;

    while (lo <= hi) {
        mid = (lo + hi) / 2;
        if (!read_packed_word_at(f, s_guess_ofs, mid, mid_word)) {
            fclose(f);
            return 0;
        }

        cmp = memcmp(word, mid_word, 5);
        if (cmp == 0) {
            fclose(f);
            return 1;
        }
        if (cmp < 0)
            hi = mid - 1;
        else
            lo = mid + 1;
    }

    fclose(f);
    return 0;
}

/* --- Loader -------------------------------------------------------------- */

int worddata_load(const char *path)
{
    FILE          *f;
    char           magic[4];
    unsigned short acnt, gcnt;
    long           guess_ofs;
    BitReader      br;
    int            n;

    f = fopen(path, "rb");
    if (!f) return 0;

    if (fread(magic, 1, 4, f) != 4 ||
        magic[0] != 'W' || magic[1] != 'R' ||
        magic[2] != 'D' || magic[3] != '5') {
        fclose(f);
        return 0;
    }

    if (fread(&acnt, 2, 1, f) != 1 || fread(&gcnt, 2, 1, f) != 1) {
        fclose(f);
        return 0;
    }

    guess_ofs = 8L + (long)(((unsigned long)acnt * 25UL + 7UL) / 8UL);

    if (worddata_answers) { _ffree(worddata_answers); worddata_answers = 0; }
    if (worddata_guesses) { _ffree(worddata_guesses); worddata_guesses = 0; }

    worddata_answers = (char __far *)_fmalloc((size_t)acnt * 5);
    if (!worddata_answers) {
        fclose(f);
        return 0;
    }

    br_init(&br, f);
    if (!unpack_words(&br, worddata_answers, (int)acnt))
        goto fail;

    worddata_answer_count = (int)acnt;
    worddata_guess_count  = (int)gcnt;
    s_guess_ofs = guess_ofs;

    if (path) {
        n = (int)strlen(path);
        if (n >= (int)sizeof(s_dat_path)) n = (int)sizeof(s_dat_path) - 1;
        memcpy(s_dat_path, path, (size_t)n);
        s_dat_path[n] = '\0';
    } else {
        s_dat_path[0] = '\0';
    }

    fclose(f);
    return 1;

fail:
    _ffree(worddata_answers);
    worddata_answers = 0;
    worddata_answer_count = 0;
    worddata_guess_count = 0;
    s_dat_path[0] = '\0';
    s_guess_ofs = 0L;
    fclose(f);
    return 0;
}
