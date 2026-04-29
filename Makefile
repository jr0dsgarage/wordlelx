# Makefile for WORDLE LX — HP 200LX / DOS target
# Requires OpenWatcom v2 on PATH.  Source the env script first:
#   . ./env-watcom.sh
# Then build:
#   wmake

CC     = wcc
LD     = wlink
CFLAGS = -0 -mm -bt=dos -os -wx -isrc

OBJS   = game.obj worddata.obj words.obj guesses.obj text_mode.obj main.obj
TARGET = WORDLELX.EXE

all : $(TARGET)

$(TARGET) : $(OBJS)
	$(LD) name $(TARGET) system dos &
	    file game.obj &
	    file worddata.obj &
	    file words.obj &
	    file guesses.obj &
	    file text_mode.obj &
	    file main.obj

game.obj : src/game.c src/game.h
	$(CC) $(CFLAGS) -fo=$@ src/game.c

worddata.obj : src/worddata.c src/worddata.h
	$(CC) $(CFLAGS) -fo=$@ src/worddata.c

words.obj : src/words.c src/words.h src/worddata.h
	$(CC) $(CFLAGS) -fo=$@ src/words.c

guesses.obj : src/guesses.c src/guesses.h src/worddata.h
	$(CC) $(CFLAGS) -fo=$@ src/guesses.c

text_mode.obj : src/text_mode.c src/game.h src/display.h
	$(CC) $(CFLAGS) -fo=$@ src/text_mode.c

main.obj : src/main.c src/game.h src/display.h src/words.h src/guesses.h
	$(CC) $(CFLAGS) -fo=$@ src/main.c

run : $(TARGET)
	/Applications/dosbox-x.app/Contents/MacOS/dosbox-x -conf dosbox-x.conf

clean : .SYMBOLIC
	rm -f $(OBJS) $(TARGET) *.err *.map
