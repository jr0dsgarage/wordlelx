/* worddata.c — loads WORDLELX.DAT into far heap memory.
 *
 * DAT format (magic "WRD5"):
 *   [0-3]  magic "WRD5"
 *   [4-5]  answer_count  (uint16 LE)
 *   [6-7]  guess_count   (uint16 LE)
 *   [8..]  answer words  packed: 5 bits/letter, MSB first, padded to byte boundary
 *   [...]  guess words   packed: same encoding, independent byte boundary
 *
 * Each letter A-Z is encoded as 0-25 in 5 bits.  The two sections are
 * byte-aligned independently so fseek can jump directly to the guess section.
 *
 * Both arrays live in far-heap segments (_fmalloc) outside the 64 KB DGROUP.
 */

#include <stdio.h>
#include <malloc.h>
#include <memory.h>
#include "worddata.h"

char __far *worddata_answers      = 0;
int         worddata_answer_count = 0;
char __far *worddata_guesses      = 0;
int         worddata_guess_count  = 0;

/* --- Bit-stream reader --------------------------------------------------- */

typedef struct {
    FILE         *f;
    unsigned long accum;  /* bit accumulator (32-bit to avoid overflow) */
    int           bits;   /* valid bits in accum                        */
} BitReader;

static void br_init(BitReader *br, FILE *f)
{
    br->f = f; br->accum = 0UL; br->bits = 0;
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

    name = "WORDLELX.DAT";
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

/* --- Loader -------------------------------------------------------------- */

int worddata_load(const char *path)
{
    FILE          *f;
    char           magic[4];
    unsigned short acnt, gcnt;
    long           guess_ofs;
    BitReader      br;

    f = fopen(path, "rb");
    if (!f) return 0;

    if (fread(magic, 1, 4, f) != 4 ||
        magic[0] != 'W' || magic[1] != 'R' ||
        magic[2] != 'D' || magic[3] != '5') {
        fclose(f); return 0;
    }

    if (fread(&acnt, 2, 1, f) != 1 || fread(&gcnt, 2, 1, f) != 1) {
        fclose(f); return 0;
    }

    /* Guess section begins after the header + packed answer bytes */
    guess_ofs = 8L + (long)(((unsigned long)acnt * 25UL + 7UL) / 8UL);

    if (worddata_answers) { _ffree(worddata_answers); worddata_answers = 0; }
    if (worddata_guesses) { _ffree(worddata_guesses); worddata_guesses = 0; }

    worddata_answers = (char __far *)_fmalloc((size_t)acnt * 5);
    worddata_guesses = (char __far *)_fmalloc((size_t)gcnt * 5);

    if (!worddata_answers || !worddata_guesses) {
        if (worddata_answers) { _ffree(worddata_answers); worddata_answers = 0; }
        if (worddata_guesses) { _ffree(worddata_guesses); worddata_guesses = 0; }
        fclose(f); return 0;
    }

    br_init(&br, f);
    if (!unpack_words(&br, worddata_answers, (int)acnt)) goto fail;

    if (fseek(f, guess_ofs, SEEK_SET) != 0) goto fail;
    br_init(&br, f);
    if (!unpack_words(&br, worddata_guesses, (int)gcnt)) goto fail;

    worddata_answer_count = (int)acnt;
    worddata_guess_count  = (int)gcnt;
    fclose(f);
    return 1;

fail:
    _ffree(worddata_answers); worddata_answers = 0;
    _ffree(worddata_guesses); worddata_guesses = 0;
    fclose(f);
    return 0;
}
