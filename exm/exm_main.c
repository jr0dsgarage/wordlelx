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

#include <stdlib.h>     /* rand, srand */
#include <time.h>       /* time        */
#include <string.h>     /* memset      */
#include <ctype.h>      /* isalpha, toupper */

/* SYSTEM_MANAGER_VERSION is in sysdefs.h but that file contains assembly
   directives OpenWatcom can't parse. Define it directly (value from sysdefs.h). */
#ifndef SYSTEM_MANAGER_VERSION
#define SYSTEM_MANAGER_VERSION  0x200
#endif

#include "game.h"
#include "words.h"
#include "exm_display.h"

/*---------------------------------------------------------------------------
 * WORDLE_DEBUG: beep-code tracing.
 * Build with -DWORDLE_DEBUG to enable.  Count beeps to find crash location:
 *   1 beep  = m_init_app done          (if missing: crash before/in m_init_app)
 *   2 beeps = Initialize() complete    (if missing: crash in InitializeCAP etc.)
 *   3 beeps = EventDispatcher running  (if missing: crash in CreateMainView)
 *   4 beeps = E_ACTIV handled          (if missing: crash in ReactivateCAP)
 *   5 beeps = E_KEY received           (if missing: crash in E_ACTIV context)
 *   6 beeps = KEYSTROKE dispatched     (if missing: crash between E_KEY/KEYSTROKE)
 *---------------------------------------------------------------------------*/
#ifdef WORDLE_DEBUG
static void debug_beeps(int n)
{
    int i, j;
    for (i = 0; i < n; i++) {
        m_beep();
        for (j = 0; j < 30000; j++)
            ;
    }
}
#define DEBUG_BEEP(n) debug_beeps(n)
#else
#define DEBUG_BEEP(n)
#endif

/*---------------------------------------------------------------------------
 * Far string literals used by the WINDOW structure and menu.
 * Must be patched for their data-segment on E_ACTIV (see FixupFarPtrs).
 *---------------------------------------------------------------------------*/
char far *msgAppName = "WORDLE LX";
char far *msgQuit    = "&Quit";
char far *msgTitle   = "";   /* empty title — prevents NULL deref in SubclassMsg */
char far *msgFkQuit  = "Quit";
char far *msgFkNew   = "New";
char far *msgFkSubm  = "Submit";

/* StringTable lets FixupFarPtrs update DS in all far string vars at once. */
char far **StringTable[] = {
    &msgAppName,
    &msgQuit,
    &msgTitle,
    &msgFkQuit,
    &msgFkNew,
    &msgFkSubm
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

/*---------------------------------------------------------------------------
 * get_ds() — returns the current data segment register value.
 * OpenWatcom uses #pragma aux; Microsoft C 6.00 uses _asm.
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

    for (i = 0; i < STRING_COUNT; i++) {
        /* Patch the segment word (high 16 bits of the far pointer) */
        *((unsigned int *)(StringTable[i]) + 1) = dataseg;
    }
}

/*---------------------------------------------------------------------------
 * Forward declarations needed before WINDOW/FKEY struct initializers
 *---------------------------------------------------------------------------*/
int far WordleCardHandler(PWINDOW Wnd, WORD Message, WORD Data, WORD Extra);
static void start_new_game(void);
static void do_submit(void);
extern WINDOW WordleCard;

/*---------------------------------------------------------------------------
 * Menu and function-key handlers
 *---------------------------------------------------------------------------*/
void far DoQuit(void)
{
    Done = TRUE;
}

void far DoNew(void)
{
    start_new_game();
    SendMsg(&WordleCard, DRAW, DRAW_ALL, 0);
}

void far DoSubmit(void)
{
    do_submit();
}

MENU WordleTopMenu[] = {
    { &msgQuit, DoQuit, 0, 0, NO_HELP },
    { 0, 0, 0, 0, 0 }
};

/* F1=Quit  F2=New  F10=Submit */
FKEY WordleFKeys[] = {
    { &msgFkQuit, DoQuit,   1,               0 },
    { &msgFkNew,  DoNew,    2,               0 },
    { &msgFkSubm, DoSubmit, 10 + FKEY_LAST,  0 }
};

/*---------------------------------------------------------------------------
 * Application window — full 640x190 screen (bottom 10px reserved by SysMgr)
 * Full-screen layout with F1=Quit, F2=New, F10=Submit fkey bar.
 * Empty title string so SubclassMsg doesn't deref NULL.
 * Simple top-level menu with a Quit item.
 *---------------------------------------------------------------------------*/
WINDOW WordleCard = {
    WordleCardHandler,              /* message handler                  */
    0, 0, 640, 190,                 /* x, y, w, h                       */
    &msgTitle, 0,                   /* Title = empty string, Data       */
    0, STYLE_NO_PARENT_KEY,         /* LogicalSize, Style               */
    NULL, WordleFKeys,              /* parent, function-key table       */
    WordleTopMenu,                  /* top-level menu                   */
    NO_HELP                         /* help topic ID                    */
};

/*---------------------------------------------------------------------------
 * Game helpers
 *---------------------------------------------------------------------------*/
static void start_new_game(void)
{
    int idx = rand() % words_count();
    game_init(&gs, words_get(idx));
    game_over_waiting = 0;
}

static void show_game_over(void)
{
    if (gs.won) {
        exm_draw_message("You won!  F2=new game");
    } else {
        static char ROM_VAR szGameOver[] = "Game over! Answer: ";
        char msg[48];
        char *p = msg;
        const char *src = szGameOver;
        while (*src) *p++ = *src++;
        src = gs.answer;
        while (*src) *p++ = *src++;
        src = "  F2=new game";
        while (*src) *p++ = *src++;
        *p = '\0';
        exm_draw_message(msg);
    }
}

/* Submit the current input (called from ENTERKEY and DoSubmit/F10). */
static void do_submit(void)
{
    if (game_over_waiting) return;
    if (gs.input_len < WORD_LEN) {
        exm_draw_message("Need 5 letters");
        return;
    }
    game_score_guess(&gs, gs.input);
    gs.input_len = 0;
    memset(gs.input, 0, sizeof(gs.input));
    exm_draw_board(&gs);
    exm_draw_keyboard(&gs);
    if (gs.over) {
        game_over_waiting = 1;
        show_game_over();
    }
}

static int handle_key(WORD data)
{
    char c = (char)(data & 0x00FF);

    /* After game over: ESC quits, any other key starts a new game */
    if (game_over_waiting) {
        if (c == (char)ESCKEY) {
            Done = TRUE;
        } else {
            DoNew();
        }
        return 1;
    }

    if (c == (char)ESCKEY)   { Done = TRUE; return 1; }
    if (c == (char)ENTERKEY) { do_submit();  return 1; }

    if (c == (char)BACKSPACEKEY || c == 127 || data == 0x0008) {
        if (gs.input_len > 0) {
            gs.input_len--;
            gs.input[gs.input_len] = '\0';
        }
        exm_draw_board(&gs);
        exm_draw_message(NULL);
        return 1;
    }

    if (isalpha((unsigned char)c) && gs.input_len < WORD_LEN) {
        gs.input[gs.input_len++] = (char)toupper((unsigned char)c);
        gs.input[gs.input_len]   = '\0';
        exm_draw_board(&gs);
        exm_draw_message(NULL);
        return 1;
    }

    return 0;  /* not a key we handle — let SubclassMsg route it (e.g. F-keys) */
}

/*---------------------------------------------------------------------------
 * Card message handler
 *
 * Receives KEYSTROKE and DRAW messages from the LHAPI card framework.
 * All unhandled messages fall through to SubclassMsg() so that the
 * framework's base-class behaviour (menus, window chrome, etc.) works.
 *---------------------------------------------------------------------------*/
int far WordleCardHandler(PWINDOW Wnd, WORD Message, WORD Data, WORD Extra)
{
    WORD subclass_data = Data;

    switch (Message) {

    case KEYSTROKE:
        /* Return immediately for keys we consume so SubclassMsg/Object never
           sees them (preventing spurious menu-accelerator matches, e.g. 'Q'
           triggering &Quit while typing a word).
           For keys we don't handle (F-keys, etc.) fall through so
           SubclassMsg/Object can dispatch them via the FKEY table.
           STYLE_NO_PARENT_KEY ensures Object won't try SendMsg(NULL,...) for
           anything that slips through unmatched. */
        DEBUG_BEEP(6);      /* 6 beeps = KEYSTROKE dispatched to our handler */
        if (handle_key(Data))
            return 1;

    case DRAW:
        /* Suppress DRAW_TITLE: our title is an empty string and we draw our
           own chrome.  SubclassMsg still needs to run for DRAW_FKEYS. */
        subclass_data &= ~DRAW_TITLE;

        if (Data & DRAW_FRAME)
            ClearRect(Wnd->x, Wnd->y, Wnd->w, Wnd->h);
        exm_draw_chrome();
        exm_draw_board(&gs);
        exm_draw_keyboard(&gs);
        if (gs.over && game_over_waiting) {
            show_game_over();
        }
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
    case E_ACTIV:
        /* Repatch far ptrs and reactivate LHAPI.
           ReactivateCAP restores graphics mode and triggers the LHAPI to
           redraw our window; we must NOT call COUGRAPH/draw functions here
           directly — that is only safe from within a DRAW message handler. */
        FixupFarPtrs();
        ReactivateCAP(&CapData);
        DEBUG_BEEP(4);      /* 4 beeps = E_ACTIV handled OK */
        break;

    case E_DEACT:
        /* Going to sleep or losing foreground: deactivate LHAPI. */
        DeactivateCAP();
        break;

    case E_TERM:
        /* System Manager is force-closing us. */
        FixupFarPtrs();
        Done = TRUE;
        break;

    case E_KEY:
        DEBUG_BEEP(5);      /* 5 beeps = E_KEY received */
        /* Forward the keystroke to whichever window has focus. */
        SendMsg(GetFocus(), KEYSTROKE,
                Fix101Key(ev->data, ev->scan),
                (WORD)ev->scan);
        break;
    }
    return 0;
}

void EventDispatcher(void)
{
    Done = FALSE;
    DEBUG_BEEP(3);          /* 3 beeps = EventDispatcher running */
    while (!Done) {
        app_event.do_event = DO_EVENT;
        m_action(&app_event);
        ProcessEvent(&app_event);
    }
}

/*---------------------------------------------------------------------------
 * Initialization / teardown
 *---------------------------------------------------------------------------*/
static void CreateMainView(void)
{
    SendMsg(&WordleCard, CREATE, CREATE_FOCUS, 0);
}

static void Initialize(void)
{
    srand((unsigned int)time(NULL));

    m_init_app(SYSTEM_MANAGER_VERSION);
    DEBUG_BEEP(1);          /* 1 beep = m_init_app reached */

    InitializeCAP(&CapData);
    m_reg_app_name(msgAppName);

    start_new_game();
    CreateMainView();       /* Display the window; triggers initial DRAW */
    DEBUG_BEEP(2);          /* 2 beeps = Initialize() complete */
}

static void Uninitialize(void)
{
    m_fini();
}

/*---------------------------------------------------------------------------
 * C entry point
 *---------------------------------------------------------------------------*/
void main(void)
{
    FixupFarPtrs();
    Initialize();
    EventDispatcher();
    Uninitialize();
}
