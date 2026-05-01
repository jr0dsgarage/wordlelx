#ifndef WORDDATA_H
#define WORDDATA_H

extern char __far *worddata_answers;
extern int         worddata_answer_count;
extern char __far *worddata_guesses;
extern int         worddata_guess_count;

/* Load WORDLELX.DAT from an explicit path. Returns 1 on success, 0 on failure. */
int worddata_load(const char *path);

/* Build "same directory as exe_path" + WORDLELX.DAT and load it.
   If exe_path has no directory component, looks in the current directory. */
int worddata_load_sibling(const char *exe_path);

/* Auto-discover WORDLELX.DAT for App Manager launches.
   Search order:
   1) C:\_DAT\WORDLELX.PTR (saved DAT directory or DAT file path)
   2) sibling of exe_path
   3) current directory
   4) recursive scan of C: (then persist PTR) */
int worddata_load_auto(const char *exe_path);

/* Validate a guess against the DAT guess list (supports packed on-disk mode). */
int worddata_guess_contains(const char *word);

#endif
