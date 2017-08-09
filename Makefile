TITLE_ID = NIDUMP001
TARGET = mDump
PSVITAIP = 192.168.1.115

MAIN_OBJS = main.o graphics.o font.o
PLUGIN_OBJS = kernel.o
HEADERS = $(wildcard *.h)

LIBS = -lSceDisplay_stub -lSceGxm_stub -lSceCtrl_stub -lSceSysmodule_stub
PLUGIN_LIBS = -Llibtaihen_stub.a -lSceSysclibForDriver_stub -lSceModulemgrForKernel_stub -lSceIofilemgrForDriver_stub

PREFIX  = arm-vita-eabi
CC      = $(PREFIX)-gcc
CFLAGS  = -Wl,-q -Wall -O3
ASFLAGS = $(CFLAGS)

all: mDump.vpk kDump.skprx

mDump.vpk: eboot.bin
	vita-mksfoex -s TITLE_ID=$(TITLE_ID) "$(TARGET)" param.sfo
	vita-pack-vpk -s param.sfo -b eboot.bin $@

eboot.bin: $(TARGET).velf
	vita-make-fself -c $< eboot.bin

mDump.velf: mDump.elf
	vita-elf-create $< $@

mDump.elf: $(MAIN_OBJS)
	$(CC) $(CFLAGS) $^ $(LIBS) -o $@

kDump.skprx: kDump.velf
	vita-make-fself -c $< $@

kDump.velf: kDump.elf
	vita-elf-create -e exports.yml $< $@

kDump.elf: $(PLUGIN_OBJS)
	$(CC) $(CFLAGS) $^ $(PLUGIN_LIBS) -o $@ -nostdlib

clean:
	@rm -rf *.velf *.elf *.vpk *.skprx $(MAIN_OBJS) $(PLUGIN_OBJS) param.sfo eboot.bin

send: eboot.bin
	curl -T eboot.bin ftp://$(PSVITAIP):1337/ux0:/app/$(TITLE_ID)/
	@echo "Sent."
