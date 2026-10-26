BUILD_DIR = ../build
RUNTIME_BUILD = $(BUILD_DIR)/runtime
MAIN_OBJ = $(RUNTIME_BUILD)/main.o
CORE_OBJS = $(addprefix $(RUNTIME_BUILD)/core/,imports.o guest.o vfpu_entry.o)
API_OBJS = $(addprefix $(RUNTIME_BUILD)/runtime/,controller.o ge.o graphics.o io.o memory.o threadman.o lwmutex.o sync.o utility.o vfpu.o)
RUNTIME_OBJS = $(MAIN_OBJ) $(CORE_OBJS) $(API_OBJS)

EE_BIN = $(BUILD_DIR)/psbridge.elf
EE_OBJS = $(RUNTIME_OBJS) $(BUILD_DIR)/game.o
EE_LIBS = -L$(GSKIT)/lib -lgskit -ldmakit -ldebug -lc -lpad
EE_INCS += -I$(CURDIR)/include -I$(GSKIT)/include
EE_LDFLAGS = -Wl,-T$(CURDIR)/linker/guest.ld
EE_ASFLAGS += -I$(abspath ..)

all: $(EE_BIN)

$(BUILD_DIR)/game.s: ../jogo.elf $(wildcard ../tools/*.py)
	cd .. && python3 tools/recompile.py jogo.elf build/game.s

$(BUILD_DIR)/game.o: $(BUILD_DIR)/game.s
	cd .. && $(EE_AS) $(EE_ASFLAGS) -call_nonpic -msingle-float build/game.s -o build/game.o

$(RUNTIME_BUILD)/main.o: main.c
	$(DIR_GUARD)
	$(EE_CC) $(EE_CFLAGS) $(EE_INCS) -c $< -o $@

$(RUNTIME_BUILD)/core/%.o: core/%.c
	$(DIR_GUARD)
	$(EE_CC) $(EE_CFLAGS) $(EE_INCS) -c $< -o $@

$(RUNTIME_BUILD)/core/guest.o: core/guest.S
	$(DIR_GUARD)
	$(EE_CC) $(EE_CFLAGS) $(EE_INCS) -c $< -o $@

$(RUNTIME_BUILD)/core/vfpu_entry.o: core/vfpu_entry.S
	$(DIR_GUARD)
	$(EE_CC) $(EE_CFLAGS) $(EE_INCS) -c $< -o $@

$(RUNTIME_BUILD)/runtime/%.o: runtime/%.c
	$(DIR_GUARD)
	$(EE_CC) $(EE_CFLAGS) $(EE_INCS) -c $< -o $@

clean:
	rm -f $(EE_BIN) $(RUNTIME_OBJS) $(BUILD_DIR)/game.o

include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal
