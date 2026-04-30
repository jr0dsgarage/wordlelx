# Makefile for WORDLE LX — HP 200LX / DOS target
# Requires OpenWatcom v2 on PATH.  Source the env script first:
#   . ./env-watcom.sh
# Then build:
#   wmake

CC     = wcc
LD     = wlink
CFLAGS = -0 -mm -bt=dos -os -wx -isrc
DOSBOX ?= dosbox-x

OUTDIR   = output
BUILDDIR = build/dos
OBJS     = $(BUILDDIR)/game.obj $(BUILDDIR)/worddata.obj $(BUILDDIR)/words.obj $(BUILDDIR)/guesses.obj $(BUILDDIR)/text_mode.obj $(BUILDDIR)/main.obj
TARGET   = $(OUTDIR)/WORDLDOS.EXE

.BEFORE
	mkdir -p $(OUTDIR)
	mkdir -p $(BUILDDIR)

all : $(TARGET)

$(TARGET) : $(OBJS)
	$(LD) name $(TARGET) system dos &
	    file $(BUILDDIR)/game.obj &
	    file $(BUILDDIR)/worddata.obj &
	    file $(BUILDDIR)/words.obj &
	    file $(BUILDDIR)/guesses.obj &
	    file $(BUILDDIR)/text_mode.obj &
	    file $(BUILDDIR)/main.obj

$(BUILDDIR)/game.obj : src/game.c src/game.h
	$(CC) $(CFLAGS) -fo=$@ src/game.c

$(BUILDDIR)/worddata.obj : src/worddata.c src/worddata.h
	$(CC) $(CFLAGS) -fo=$@ src/worddata.c

$(BUILDDIR)/words.obj : src/words.c src/words.h src/worddata.h
	$(CC) $(CFLAGS) -fo=$@ src/words.c

$(BUILDDIR)/guesses.obj : src/guesses.c src/guesses.h src/worddata.h
	$(CC) $(CFLAGS) -fo=$@ src/guesses.c

$(BUILDDIR)/text_mode.obj : src/text_mode.c src/game.h src/display.h
	$(CC) $(CFLAGS) -fo=$@ src/text_mode.c

$(BUILDDIR)/main.obj : src/main.c src/game.h src/display.h src/words.h src/guesses.h
	$(CC) $(CFLAGS) -fo=$@ src/main.c

run : $(TARGET)
	$(DOSBOX) -conf dosbox-x.conf

clean : .SYMBOLIC
	rm -f $(OBJS) $(TARGET) $(OUTDIR)/WORDLELX.EXE *.err *.map
