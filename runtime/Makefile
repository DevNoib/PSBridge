EE_BIN = ../build/psbridge.elf
EE_OBJS = main.o guest.o ../build/game.o
EE_LIBS = -ldebug -lc
EE_LDFLAGS = -Wl,-T$(CURDIR)/guest.ld
EE_ASFLAGS += -I$(abspath ..)

all: $(EE_BIN)

../build/game.s: ../jogo.elf $(wildcard ../tools/*.py)
	cd .. && python3 tools/recompile.py jogo.elf build/game.s

../build/game.o: ../build/game.s
	cd .. && $(EE_AS) $(EE_ASFLAGS) -call_nonpic -msingle-float build/game.s -o build/game.o

clean:
	rm -f $(EE_BIN) main.o guest.o

include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal
