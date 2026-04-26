# Makefile for WORDLE LX — HP 200LX / DOS target
# Requires OpenWatcom v2 on PATH.  Source the env script first:
#   . ./env-watcom.sh
# Then build:
#   wmake

CC     = wcc
LD     = wlink
CFLAGS = -0 -mm -bt=dos -os -wx -isrc

OBJS   = game.obj words.obj text_mode.obj main.obj
TARGET = WORDLELX.EXE

all : $(TARGET)

$(TARGET) : $(OBJS)
	$(LD) name $(TARGET) system dos &
	    file game.obj &
	    file words.obj &
	    file text_mode.obj &
	    file main.obj

game.obj : src/game.c src/game.h
	$(CC) $(CFLAGS) -fo=$@ src/game.c

words.obj : src/words.c src/words.h
	$(CC) $(CFLAGS) -fo=$@ src/words.c

text_mode.obj : src/text_mode.c src/game.h src/display.h
	$(CC) $(CFLAGS) -fo=$@ src/text_mode.c

main.obj : src/main.c src/game.h src/display.h src/words.h
	$(CC) $(CFLAGS) -fo=$@ src/main.c

run : $(TARGET)
	/Applications/dosbox-x.app/Contents/MacOS/dosbox-x -conf dosbox-x.conf

clean : .SYMBOLIC
	rm -f $(OBJS) $(TARGET) *.err *.map
