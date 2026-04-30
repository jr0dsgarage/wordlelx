/*===========================================================================
 * exm_main.c - Wordle LX EXM entry point and System Manager event loop
 *
 * Registers with the HP 200LX System Manager via the LHAPI (CAP) framework,
 * drives the Wordle game loop through System Manager events, and delegates
 * all rendering to exm_display.c.
 *
 * Architecture:
 *   main()             → Initialize → EventDispatcher → Uninitialize
 *   EventDispatcher()  → calls m_action() in a loop, routes to ProcessEvent()
 *   ProcessEvent()     → handles E_KEY (dispatch to card via SendMsg),
 *                         E_ACTIV/E_REFRESH (ReactivateCAP + redraw),
 *                         E_DEACT (DeactivateCAP), E_TERM (set Done)
 *   WordleCardHandler() → handles KEYSTROKE (game logic + redraw),
 *                          DRAW (full screen refresh via exm_display)
 *
 * All game state lives in the global `gs` (GameState). Game logic is
 * in game.c / words.c — unchanged from the DOS .EXE version.
 *===========================================================================*/

#include "cap2.h"       /* LHAPI aliases: CAPBLOCK, PWINDOW, WINDOW, etc. */
#include "interfac.h"   /* m_init_app, m_fini, m_action, m_reg_app_name   */
#include "event.h"      /* EVENT, event_kind, edo_event                    */
#include "cougraph.h"   /* G_Mode, G_CGAGRAPH, G_RESTORE                  */
#include "keytab.h"     /* ENTERKEY, BACKSPACEKEY, ESCKEY                  */

#include <stdio.h>      /* fopen, fclose, fread, fwrite, sprintf */
#include <stdlib.h>     /* rand, srand */
#include <time.h>       /* time        */
#include <string.h>     /* memset      */
#include <ctype.h>      /* isalpha, toupper */
#include <direct.h>     /* getcwd */
#include <dos.h>        /* _dos_findfirst, _dos_findnext, find_t */

extern char near *_LpPgmName;

/*---------------------------------------------------------------------------
 * PTR file — persists the DAT directory across cold boots.
 * Written to / read from CWD, which is always C:\_DAT on HP 200LX EXMs.
 *---------------------------------------------------------------------------*/
#define PTR_FILE "WORDLELX.PTR"

static void ptr_save(const char *dir)
{
    FILE *f = fopen(PTR_FILE, "w");
    if (f) { fputs(dir, f); fclose(f); }
}

static int ptr_load(char *dir, int maxlen)
{
    FILE *f = fopen(PTR_FILE, "r");
    int   n;
    if (!f) return 0;
    n = (int)fread(dir, 1, maxlen - 1, f);
    fclose(f);
    while (n > 0 && (dir[n-1] == '\r' || dir[n-1] == '\n' || dir[n-1] == ' '))
        n--;
    dir[n] = '\0';
    return n > 0;
}

static int try_load_from_dir(const char *dir);   /* defined below */

/*---------------------------------------------------------------------------
 * scan_drive — search root + all immediate subdirectories of C:\ for the DAT.
 * Used as last resort when _LpPgmName is empty and PATH is not set.
 *---------------------------------------------------------------------------*/
static int scan_drive(char *found_dir, int maxlen)
{
    struct find_t fb;
    char          pat[16];
    char          dir[64];
    int           n;

    if (try_load_from_dir("C:\\")) {
        if (found_dir) { found_dir[0] = 'C'; found_dir[1] = ':'; found_dir[2] = '\\'; found_dir[3] = '\0'; }
        return 1;
    }

    pat[0]='C'; pat[1]=':'; pat[2]='\\'; pat[3]='*'; pat[4]='\0';
    if (_dos_findfirst(pat, _A_SUBDIR, &fb) != 0) return 0;
    do {
        if (!(fb.attrib & _A_SUBDIR)) continue;
        if (fb.name[0] == '.') continue;
        n = sprintf(dir, "C:\\%s", fb.name);
        if (n < 0 || n >= (int)sizeof(dir))
            continue;
        if (try_load_from_dir(dir)) {
            if (found_dir) {
                int n = (int)strlen(dir);
                if (n < maxlen) { memcpy(found_dir, dir, n+1); }
            }
            _dos_findclose(&fb);
            return 1;
        }
    } while (_dos_findnext(&fb) == 0);
    _dos_findclose(&fb);
    return 0;
}

/* SYSTEM_MANAGER_VERSION is in sysdefs.h but that file contains assembly
   directives OpenWatcom can't parse. Define it directly (value from sysdefs.h). */
#ifndef SYSTEM_MANAGER_VERSION
#define SYSTEM_MANAGER_VERSION  0x200
#endif

#include "game.h"
#include "words.h"
#include "guesses.h"
#include "worddata.h"
#include "exm_display.h"

/*---------------------------------------------------------------------------
 * Far string literals used by the WINDOW structure and menu.
 * Must be patched for their data-segment on E_ACTIV (see FixupFarPtrs).
 *---------------------------------------------------------------------------*/
char far *msgAppName = "WORDLE LX";
char far *msgTitle   = "";   /* empty title — prevents NULL deref in SubclassMsg */
char far *msgFkNew   = "New";
char far *msgFkQuit  = "Quit";

/* StringTable lets FixupFarPtrs update DS in all far string vars at once. */
char far **StringTable[] = {
    &msgAppName,
    &msgTitle,
    &msgFkNew,
    &msgFkQuit
};

#define STRING_COUNT (sizeof(StringTable) / sizeof(StringTable[0]))

/*---------------------------------------------------------------------------
 * Global state
 *---------------------------------------------------------------------------*/
EVENT    app_event;   /* reused for every m_action() call */
CAPBLOCK CapData;     /* LHAPI capability block           */
GameState gs;         /* live game state                  */
BOOL     Done;        /* event loop termination flag       */
int      game_over_waiting; /* 1 = game ended, waiting for keypress */
char     pending_msg[48];   /* message shown in next DRAW; empty = none */

/* DAT load state: 0=not yet tried, 1=tried+failed, 2=loaded */
static int dat_load_state = 0;

/* Default two-line credit shown when no game message is active */
static char szCredit1[] = "Programmed by Jarrod Kozeal";
static char szCredit2[] = "using Claude Code - 2026";

/*---------------------------------------------------------------------------
 * get_ds() — returns the current data segment register value.
 *---------------------------------------------------------------------------*/
#ifdef __WATCOMC__
unsigned short get_ds(void);
#pragma aux get_ds = "mov ax, ds" value [ax] modify [ax];
#else
static unsigned short get_ds(void)
{
    unsigned short v;
    _asm { mov ax, ds; mov v, ax }
    return v;
}
#endif

/*---------------------------------------------------------------------------
 * FixupFarPtrs — repatch far string segment registers after task swap.
 * (In small model, far pointers to DS strings hardcode a DS value that
 *  becomes stale when the System Manager restores DS for another task.)
 *---------------------------------------------------------------------------*/
static void FixupFarPtrs(void)
{
    unsigned int dataseg = get_ds();
    int i;
    for (i = 0; i < STRING_COUNT; i++)
        *((unsigned int *)(StringTable[i]) + 1) = dataseg;
}

/*---------------------------------------------------------------------------
 * Forward declarations needed before WINDOW/FKEY struct initializers
 *---------------------------------------------------------------------------*/
int far WordleCardHandler(PWINDOW Wnd, WORD Message, WORD Data, WORD Extra);
static void start_new_game(void);
static void do_submit(void);
extern WINDOW WordleCard;

/*---------------------------------------------------------------------------
 * Function-key handlers
 *---------------------------------------------------------------------------*/
void far DoQuit(void)
{
    Done = TRUE;
}

void far DoNew(void)
{
    if (dat_load_state == 1) {
        dat_load_state = 0;
        SendMsg(&WordleCard, DRAW, DRAW_ALL, 0);
        return;
    }
    start_new_game();
    SendMsg(&WordleCard, DRAW, DRAW_ALL, 0);
}

/* F1=New  F10=Quit */
FKEY WordleFKeys[] = {
    { &msgFkNew,  DoNew,  1,               0 },
    { &msgFkQuit, DoQuit, 10 + FKEY_LAST,  0 }
};

/*---------------------------------------------------------------------------
 * Application window — full 640x190 screen (bottom 10px reserved by SysMgr)
 *---------------------------------------------------------------------------*/
WINDOW WordleCard = {
    WordleCardHandler,
    0, 0, 640, 190,
    &msgTitle, 0,
    0, 0,
    NULL, WordleFKeys,
    NO_MENU,
    NO_HELP
};

/*---------------------------------------------------------------------------
 * Game helpers
 *---------------------------------------------------------------------------*/

/* Try all three strategies to find WORDLELX.DAT.  Called from DRAW so the
   System Manager has fully activated the app before we attempt path lookups. */
static int try_load_from_dir(const char *dir)
{
    char path[80];
    int n;

    if (!dir || !dir[0])
        return worddata_load("WORDLELX.DAT");

    n = sprintf(path, "%s%sWORDLELX.DAT", dir,
                (dir[strlen(dir) - 1] == '\\') ? "" : "\\");
    if (n < 0 || n >= (int)sizeof(path))
        return 0;

    return worddata_load(path);
}

static void worddata_try_load(void)
{
    char cwd[64];
    int  ok = 0;

    if (dat_load_state == 2) return;

    cwd[0] = '\0';
    getcwd(cwd, sizeof(cwd));

    if (!ok) {
        char saved[64];
        if (ptr_load(saved, sizeof(saved)))
            ok = try_load_from_dir(saved);
    }

    if (!ok && _LpPgmName && _LpPgmName[0])
        ok = worddata_load_sibling(_LpPgmName);

    if (!ok)
        ok = try_load_from_dir(cwd);

    if (!ok) {
        char found[64];
        found[0] = '\0';
        ok = scan_drive(found, sizeof(found));
        if (ok) ptr_save(found);
    }

    if (ok) {
        dat_load_state = 2;
        start_new_game();
    } else {
        dat_load_state = 1;
    }
}

static void start_new_game(void)
{
    int         idx = rand() % words_count();
    const char *w   = words_get(idx);
    game_init(&gs, w ? w : "XXXXX");
    game_over_waiting = 0;
    pending_msg[0] = '\0';
}

static void show_game_over(void)
{
    /* CP437: 0x01=☺ smiley, 0x02=☻ dark smiley (used as sad face) */
    if (gs.won) {
        exm_draw_message2("You've won! \x01", "F1 = New Game");
    } else {
        static char szAnswerLine[32];
        const char *src;
        char *d = szAnswerLine;
        src = "The correct word was: ";
        while (*src) *d++ = *src++;
        src = gs.answer;
        while (*src) *d++ = *src++;
        *d = '\0';
        exm_draw_message3("You've lost! \x02", szAnswerLine, "F1 = New Game");
    }
}

static void do_submit(void)
{
    const char *s;
    char *d;
    if (game_over_waiting) return;
    if (gs.input_len < WORD_LEN) {
        s = "Need 5 letters";
        d = pending_msg;
        while (*s) *d++ = *s++;
        *d = '\0';
        SendMsg(&WordleCard, DRAW, DRAW_ALL, 0);
        return;
    }
    if (!words_contains(gs.input) && !guesses_is_valid(gs.input)) {
        d = pending_msg;
        *d++ = '"';
        s = gs.input; while (*s) *d++ = *s++;
        s = "\" is not a valid word!";
        while (*s) *d++ = *s++;
        *d = '\0';
        gs.input_len = 0;
        memset(gs.input, 0, sizeof(gs.input));
        SendMsg(&WordleCard, DRAW, DRAW_ALL, 0);
        return;
    }
    game_score_guess(&gs, gs.input);
    gs.input_len = 0;
    memset(gs.input, 0, sizeof(gs.input));
    pending_msg[0] = '\0';
    if (gs.over)
        game_over_waiting = 1;
    SendMsg(&WordleCard, DRAW, DRAW_ALL, 0);
}

/* Map standard PC XT scan codes for the 26 letters → uppercase ASCII.
   Fallback for when Fix101Key returns 0 in the ASCII byte (HP 200LX quirk). */
static char scan_to_letter(WORD scan)
{
    switch (scan) {
    case 0x10: return 'Q'; case 0x11: return 'W'; case 0x12: return 'E';
    case 0x13: return 'R'; case 0x14: return 'T'; case 0x15: return 'Y';
    case 0x16: return 'U'; case 0x17: return 'I'; case 0x18: return 'O';
    case 0x19: return 'P'; case 0x1E: return 'A'; case 0x1F: return 'S';
    case 0x20: return 'D'; case 0x21: return 'F'; case 0x22: return 'G';
    case 0x23: return 'H'; case 0x24: return 'J'; case 0x25: return 'K';
    case 0x26: return 'L'; case 0x2C: return 'Z'; case 0x2D: return 'X';
    case 0x2E: return 'C'; case 0x2F: return 'V'; case 0x30: return 'B';
    case 0x31: return 'N'; case 0x32: return 'M';
    default:   return 0;
    }
}

static int handle_key(WORD data, WORD scan)
{
    char c = (char)(data & 0x00FF);
    char letter;
    /* HP 200LX sends ASCII in the HIGH byte; low byte is always 0x00 */
    if (c == 0)
        c = (char)((data >> 8) & 0xFF);

    /* After game over: swallow all keys; only F1 (caught in WordleCardHandler) starts new game */
    if (game_over_waiting) {
        return 1;
    }

    if (c == (char)ESCKEY)   { Done = TRUE;   return 1; }
    if (c == (char)ENTERKEY) { do_submit();   return 1; }

    if (c == (char)BACKSPACEKEY || c == 127 || data == 0x0008) {
        if (gs.input_len > 0) {
            gs.input_len--;
            gs.input[gs.input_len] = '\0';
        }
        pending_msg[0] = '\0';
        exm_draw_active_row(&gs);
        exm_draw_message2(szCredit1, szCredit2);
        return 1;
    }

    letter = 0;
    if (isalpha((unsigned char)c))
        letter = (char)toupper((unsigned char)c);
    else
        letter = scan_to_letter(scan);

    if (letter && gs.input_len < WORD_LEN) {
        gs.input[gs.input_len++] = letter;
        gs.input[gs.input_len]   = '\0';
        pending_msg[0] = '\0';
        exm_draw_active_row(&gs);
        exm_draw_message2(szCredit1, szCredit2);
        return 1;
    }

    return 0;
}

/*---------------------------------------------------------------------------
 * Card message handler
 *---------------------------------------------------------------------------*/
int far WordleCardHandler(PWINDOW Wnd, WORD Message, WORD Data, WORD Extra)
{
    WORD subclass_data = Data;

    switch (Message) {

    case KEYSTROKE:
        /* Always allow exit keys even when data is missing — otherwise
           the user is trapped with no way to close the app. */
        if (Extra == 0x003B) { DoNew();     return 1; }  /* F1  */
        if (Extra == 0x0044) { Done = TRUE; return 1; }  /* F10 */
        {
            char _c = (char)(Data & 0xFF);
            if (_c == 0) _c = (char)((Data >> 8) & 0xFF);
            if (_c == (char)ESCKEY) { Done = TRUE; return 1; }
        }
        if (worddata_answer_count == 0) return 1;   /* no data, swallow other keys */
        if (handle_key(Data, Extra))
            return 1;
        break;

    case DRAW:
        subclass_data &= ~DRAW_TITLE;
        worddata_try_load();
        if (worddata_answer_count == 0) {
            exm_draw_message2("WORDLELX.DAT not found!", "Place next to EXM  F1=Retry");
            break;
        }
        if (Data & DRAW_FRAME)
            ClearRect(Wnd->x, Wnd->y, Wnd->w, Wnd->h);
        exm_draw_board(&gs);
        exm_draw_chrome();
        exm_draw_keyboard(&gs);
        if (gs.over && game_over_waiting) {
            show_game_over();
        } else if (pending_msg[0]) {
            exm_draw_message(pending_msg);
        } else {
            exm_draw_message2(szCredit1, szCredit2);
        }
        break;

    default:
        break;
    }

    return SubclassMsg(Object, Wnd, Message, subclass_data, Extra);
}

/*---------------------------------------------------------------------------
 * Event dispatcher — the main event loop
 *---------------------------------------------------------------------------*/
static int ProcessEvent(EVENT *ev)
{
    switch (ev->kind) {

    case E_REFRESH:
        FixupFarPtrs();
        ReactivateCAP(&CapData);
        break;

    case E_ACTIV:
        FixupFarPtrs();
        ReactivateCAP(&CapData);
        break;

    case E_DEACT:
        DeactivateCAP();
        break;

    case E_TERM:
        FixupFarPtrs();
        Done = TRUE;
        break;

    case E_KEY:
        /* Match HexCalc exactly: pass data as-is (Fix101Key identity),
           let LHAPI/SubclassMsg use Extra (scan) for non-ASCII keys. */
        SendMsg(GetFocus(), KEYSTROKE, (WORD)ev->data, (WORD)ev->scan);
        break;

    default:
        break;
    }
    return 0;
}

void EventDispatcher(void)
{
    Done = FALSE;
    while (!Done) {
        app_event.do_event = DO_EVENT;
        m_action(&app_event);
        ProcessEvent(&app_event);
    }
}

/*---------------------------------------------------------------------------
 * Initialization / teardown
 *---------------------------------------------------------------------------*/
static void Initialize(void)
{
    memset(&gs, 0, sizeof(gs));
    srand((unsigned int)time(NULL));
    m_init_app(SYSTEM_MANAGER_VERSION);
    InitializeCAP(&CapData);
    SetMenuFont(FONT_NORMAL);
    SetFont(FONT_NORMAL);
    RegisterFont(FONT_LARGE);
    m_reg_app_name(msgAppName);
    exm_init_fonts();
    worddata_try_load();
    SendMsg(&WordleCard, CREATE, CREATE_FOCUS, 0);
}

void main(void)
{
    FixupFarPtrs();
    Initialize();
    EventDispatcher();
    m_fini();
}
