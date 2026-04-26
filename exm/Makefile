###############################################################################
# Wordle LX - EXM Build (HP 200LX System Manager Application)
#
# PREREQUISITES:
#   1. Microsoft C 6.00 (cl.exe, link.exe, lib.exe) on PATH
#      - First try: OpenWatcom wcc with -cc flag (see WATCOM section below)
#      - Fallback: MC6.00 via DOSBox-X with mounted source directory
#   2. EXM SDK cloned to $(EXM_ROOT) (default: ../../EXM)
#      git clone https://github.com/200lx/EXM ../../EXM
#   3. HP 200LX TKERNEL.EXE and CG.COM or CGAGRAPH.COM for target testing
#
# BUILD TARGETS:
#   make             - default TKernel (PC emulator) build
#   make OPT=tko     - TKernel Optimized (no CodeView)
#   make OPT=emul    - Emulator/bb.lib build (DOSBox-X without TKernel TSR)
#   make OPT=real    - Real HP 200LX hardware build
#   make OPT=cvo     - TKernel with CodeView debug symbols
#   make clean       - Remove build artifacts
#
# USAGE EXAMPLE (DOSBox-X development):
#   nmake OPT=emul
#   copy WORDLELX.EXM C:\HPCALC\  (or wherever your 200LX share is)
###############################################################################

# ---- Paths ------------------------------------------------------------------
EXM_ROOT = ..\..\EXM
INC_PATH = $(EXM_ROOT)\HEADERS
LIB_PATH = $(EXM_ROOT)\LIBS
SRC_PATH = ..\src

# ---- App config -------------------------------------------------------------
APP_NAME = WORDLELX
EXEFILE  = $(APP_NAME).EXE
EXMFILE  = $(APP_NAME).EXM
E2M      = $(EXM_ROOT)\SAMPLES\HEXCALC\E2M.EXE

# ---- Warning level defaults -------------------------------------------------
!IFNDEF WARNING_LEVEL
WARNING_LEVEL = 1
!ENDIF

STACK_SIZE = 0x1000

# ---- Build-type selection ---------------------------------------------------
# tko - TKernel Optimized (default for PC development with TKERNEL.EXE loaded)
!IF "$(OPT)"=="tko" || "$(OPT)"==""
CFLAGS   = /AS /W$(WARNING_LEVEL) /Ze /DPCKEYBOARD /DTKERNEL /DAPI2 /G1 /Oswl /Gs
LFLAGS   = /CP:0xffff /NOI /NOE /SE:0x80 /ST:$(STACK_SIZE)
CAP_LIB  = $(LIB_PATH)\captko.lib
SYS_LIBS = $(LIB_PATH)\tsr.lib $(LIB_PATH)\mlhstub.lib
!ENDIF

# cvo - TKernel with CodeView debug info
!IF "$(OPT)"=="cvo"
CFLAGS   = /AS /W$(WARNING_LEVEL) /Ze /DPCKEYBOARD /DTKERNEL /DAPI2 /G1 /Oswl /Gs /Zi /Zd
LFLAGS   = /CP:0xffff /NOI /NOE /SE:0x80 /ST:$(STACK_SIZE) /CO
CAP_LIB  = $(LIB_PATH)\captko.lib
SYS_LIBS = $(LIB_PATH)\tsr.lib $(LIB_PATH)\mlhstub.lib
!ENDIF

# emul - DOSBox-X emulator build (no TKERNEL TSR needed, uses bb.lib)
!IF "$(OPT)"=="emul"
CFLAGS   = /AS /W$(WARNING_LEVEL) /Ze /Oswl /Gs /G1 /DNDEBUG /DEMULATOR /DPCKEYBOARD /DAPI2
LFLAGS   = /CP:0xffff /NOI /NOE /NOD /MAP /SE:0x80 /ST:$(STACK_SIZE)
CAP_LIB  = $(LIB_PATH)\capemu.lib
SYS_LIBS = $(LIB_PATH)\bb.lib $(LIB_PATH)\mlhstub.lib
!ENDIF

# real - Real HP 200LX hardware (caprea.lib, bb.lib)
!IF "$(OPT)"=="real"
CFLAGS   = /AS /W$(WARNING_LEVEL) /Ze /Oswl /Gs /G1 /DNDEBUG /DAPI2 /DOFFICIAL
LFLAGS   = /CP:0xffff /NOI /NOE /NOD /MAP /SE:0x80 /ST:$(STACK_SIZE)
CAP_LIB  = $(LIB_PATH)\caprea.lib
SYS_LIBS = $(LIB_PATH)\bb.lib $(LIB_PATH)\mlhstub.lib
!ENDIF

# ---- Source objects ---------------------------------------------------------
# Game logic objects (compiled from ../src/ with same CFLAGS)
GAME_OBJS = game.obj words.obj

# EXM-specific objects
EXM_OBJS  = exm_main.obj exm_display.obj

ALL_OBJS  = $(EXM_OBJS) $(GAME_OBJS)

# ---- Default target ---------------------------------------------------------
all: $(EXMFILE)

# ---- EXM conversion ---------------------------------------------------------
$(EXMFILE): $(EXEFILE)
	$(E2M) $(EXEFILE)

# ---- Link EXE ---------------------------------------------------------------
$(EXEFILE): $(ALL_OBJS)
	link $(LFLAGS) @<<
$(ALL_OBJS: =+),$(EXEFILE),,$(SYS_LIBS) $(CAP_LIB);
<<NOKEEP

# ---- Compile rules ----------------------------------------------------------
exm_main.obj: exm_main.c exm_display.h
	cl /c /I$(INC_PATH) /I$(SRC_PATH) $(CFLAGS) exm_main.c

exm_display.obj: exm_display.c exm_display.h
	cl /c /I$(INC_PATH) /I$(SRC_PATH) $(CFLAGS) exm_display.c

game.obj: $(SRC_PATH)\game.c $(SRC_PATH)\game.h
	cl /c /I$(INC_PATH) /I$(SRC_PATH) $(CFLAGS) /Fo$@ $(SRC_PATH)\game.c

words.obj: $(SRC_PATH)\words.c $(SRC_PATH)\words.h
	cl /c /I$(INC_PATH) /I$(SRC_PATH) $(CFLAGS) /Fo$@ $(SRC_PATH)\words.c

# ---- Cleanup ----------------------------------------------------------------
clean:
	-del *.obj
	-del *.lib
	-del *.exe
	-del *.exm
	-del *.map

###############################################################################
# OPENWTACOM NOTES (if you don't have Microsoft C 6.00):
#
# OpenWatcom may work with Microsoft calling convention flags:
#   WCC  = wcc
#   WFLAGS = -ms -3 -cc -DPCKEYBOARD -DTKERNEL -DAPI2 -os
#   (then link with wlink against the EXM .lib files)
#
# However, ABI compatibility with the pre-built EXM .lib files is not
# guaranteed. If linking fails, obtain Microsoft C 6.00.
#
# Microsoft C 6.00 is available from various archive sources and can
# be run in DOSBox-X with the source directory mounted.
###############################################################################
