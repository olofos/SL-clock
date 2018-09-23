V=@

SOURCES := fonts.c journey.c journey-task.c config.c framebuffer.c display.c display-message.c \
    json.c json-util.c json-http.c  log.c logo-paw-64x64.c sntp.c sh1106.c timezone-db.c uart.c user_main.c wifi-task.c wifi-list.c wifi-logic.c \
    i2c-master.c http-server-task.c

TARGET=user

SRCDIR := src
OBJDIR := obj
DEPDIR := .deps
TSTDIR := test
TSTOBJDIR := test/obj
TSTBINDIR := test/bin
RESULTDIR := test/results
UNITYDIR := unity

BUILD_DIRS = $(OBJDIR) $(DEPDIR) $(RESULTDIR) $(TSTOBJDIR) $(TSTBINDIR)

SRC := $(SOURCES:%.c=$(SRCDIR)/%.c)
OBJ := $(SOURCES:%.c=$(OBJDIR)/%.o)
DEPS := $(SOURCES:%.c=$(DEPDIR)/%.d)

SOURCES_TST = $(wildcard $(TSTDIR)/*.c)


AR = xtensa-lx106-elf-ar
CC = xtensa-lx106-elf-gcc
NM = xtensa-lx106-elf-nm
OBJCOPY = xtensa-lx106-elf-objcopy
OBJDUMP = xtensa-lx106-elf-objdump

CFLAGS = -DFREERTOS=1 -std=gnu99 -Os -g -Wpointer-arith -Wundef -Wall -Wl,-EL -fno-inline-functions -nostdlib -mlongcalls -mtext-section-literals \
         -ffunction-sections -fdata-sections -fno-builtin-printf -fno-jump-tables $(INCLUDES)

LDFILE = ld/eagle.app.v6.ld

LDFLAGS = -L$(SDK_PATH)/lib -Lhttp-sm/lib -Wl,--gc-sections -nostdlib -T$(LDFILE) -Wl,--no-check-sections -u call_user_start -Wl,-static -Wl,--start-group -lcirom -lcrypto -lespconn -lespnow -lfreertos -lgcc -lhal -llwip -lmain -lmirom -lnet80211 -lnopoll -lphy -lpp -lpwm -lsmartconfig -lspiffs -lssl -lwpa obj/libuser.a -lhttp-sm -lwps -Wl,--end-group

INCLUDES := $(INCLUDES)
INCLUDES += -Ihttp-sm/include
INCLUDES += -I$(SDK_PATH)/include
INCLUDES += -I$(SDK_PATH)/extra_include
INCLUDES += -I$(SDK_PATH)/driver_lib/include
INCLUDES += -I$(SDK_PATH)/include/espressif
INCLUDES += -I$(SDK_PATH)/include/lwip
INCLUDES += -I$(SDK_PATH)/include/lwip/ipv4
INCLUDES += -I$(SDK_PATH)/include/lwip/ipv6
INCLUDES += -I$(SDK_PATH)/include/nopoll
INCLUDES += -I$(SDK_PATH)/include/spiffs
INCLUDES += -I$(SDK_PATH)/include/ssl
INCLUDES += -I$(SDK_PATH)/include/json
INCLUDES += -I$(SDK_PATH)/include/openssl

TST_CC = gcc
TST_CFLAGS = -Wall -I$(UNITYDIR) -I$(SRCDIR) -g

TST_RESULTS = $(patsubst $(TSTDIR)/test_%.c,$(RESULTDIR)/test_%.txt,$(SOURCES_TST))

.PHONY: all bin flash clean erase spiffs-flash spiffs-image test build_dirs

all: build_dirs eagle.app.flash.bin

-include $(DEPS)

http-sm/lib/libhttp-sm.a:
	$(V)$(MAKE) CC="$(CC)" AR="$(AR)" CFLAGS="$(CFLAGS) -I../src/ -DLOG_SYS=LOG_SYS_HTTP" V=$(V) -Chttp-sm/ lib/libhttp-sm.a

$(OBJDIR)/%.o : $(SRCDIR)/%.c
	@echo CC $@
	$(V)$(CC) $(CFLAGS) -c $< -o $@
	$(V)$(CC) -MM -MT $@ $(CFLAGS) $< > $(DEPDIR)/$*.d

$(OBJDIR)/libuser.a: $(OBJ)
	@echo AR $@
	$(V)$(AR) ru $@ $^

$(OBJDIR)/user.elf : $(OBJDIR)/libuser.a http-sm/lib/libhttp-sm.a
	@echo LINKING $@
	$(V)$(CC) $(LDFLAGS) -o $@

eagle.app.flash.bin: $(OBJDIR)/user.elf
	@echo OBJCOPY $@
	$(V)$(OBJCOPY) --only-section .text -O binary obj/user.elf eagle.app.v6.text.bin
	$(V)$(OBJCOPY) --only-section .rodata -O binary obj/user.elf eagle.app.v6.rodata.bin
	$(V)$(OBJCOPY) --only-section .data -O binary obj/user.elf eagle.app.v6.data.bin
	$(V)$(OBJCOPY) --only-section .irom0.text -O binary obj/user.elf eagle.app.v6.irom0text.bin
	$(V)python $(SDK_PATH)/tools/gen_appbin.py obj/user.elf 0 2 0 4
	$(V)mv eagle.app.v6.text.bin bin/eagle.app.v6.text.bin
	$(V)mv eagle.app.v6.rodata.bin bin/eagle.app.v6.rodata.bin
	$(V)mv eagle.app.v6.data.bin bin/eagle.app.v6.data.bin
	$(V)mv eagle.app.v6.irom0text.bin bin/eagle.app.v6.irom0text.bin
	$(V)mv eagle.app.flash.bin bin/eagle.app.flash.bin

flash: eagle.app.flash.bin
	esptool.py -p /dev/ttyUSB0 --baud 921600 write_flash -fs 32m -fm dio -ff 40m 0x00000 bin/eagle.app.flash.bin 0x20000 bin/eagle.app.v6.irom0text.bin 0x3fc000 $(SDK_PATH)/bin/esp_init_data_default.bin

spiffs-image:
	../mkspiffs/mkspiffs -b 4096 -p 128 -s 196608 -c data/ bin/spiffs.bin

spiffs-flash: spiffs-image
	esptool.py  -p /dev/ttyUSB0 --baud 921600 write_flash -fs 32m 0x100000 bin/spiffs.bin

erase:
	esptool.py -p /dev/ttyUSB0 --baud 921600 erase_flash


test: build_dirs $(TST_RESULTS)
	@echo "-----------------------"
	@echo "IGNORE:" `grep -o ':IGNORE' $(RESULTDIR)/*.txt|wc -l`
	@echo "-----------------------"
	@echo `grep -s ':IGNORE' $(RESULTDIR)/*.txt`
	@echo "\n-----------------------"
	@echo "FAIL:" `grep -o ':FAIL' $(RESULTDIR)/*.txt|wc -l`
	@echo "-----------------------"
	@echo `grep -s ':FAIL' $(RESULTDIR)/*.txt`
	@echo "\n-----------------------"
	@echo "PASS:" `grep -o ':PASS' $(RESULTDIR)/*.txt|wc -l`
	@echo "-----------------------"
	@echo
	@! grep -s FAIL $(RESULTDIR)/*.txt 2>&1 1>/dev/null

build_dirs:
	$(V)mkdir -p $(BUILD_DIRS)


$(RESULTDIR)/%.txt: $(TSTBINDIR)/%
	@echo Running $<
	@echo
	$(V)./$< > $@ || true


$(TSTOBJDIR)/%.o : $(TSTDIR)/%.c
	$(V)echo CC $@
	$(V)$(TST_CC) $(TST_CFLAGS) -c $< -o $@

$(TSTOBJDIR)/%.o : $(SRCDIR)/%.c
	@echo CC $@
	$(V)$(TST_CC) $(TST_CFLAGS) -c $< -o $@

$(TSTOBJDIR)/%.o : $(UNITYDIR)/%.c
	@echo CC $@
	$(V)$(TST_CC) $(TST_CFLAGS) -c $< -o $@

$(TSTBINDIR)/test_%: $(TSTOBJDIR)/test_%.o $(TSTOBJDIR)/%.o $(TSTOBJDIR)/unity.o
	@echo CC $@
	$(V)$(TST_CC) -o $@ $^

clean:
	@echo Cleaning
	$(V)-rm -f $(OBJ) $(OBJDIR)/libuser.a $(OBJDIR)/user.elf $(TSTOBJDIR)/*.o $(TSTBINDIR)/test_* $(RESULTDIR)/*.txt $(DEPDIR)/*.d bin/eagle.app.v6.text.bin bin/eagle.app.v6.rodata.bin bin/eagle.app.v6.data.bin bin/eagle.app.v6.irom0text.bin bin/eagle.app.flash.bin
	$(V)$(MAKE) -Chttp-sm clean

.PRECIOUS: $(TSTBINDIR)/test_%
.PRECIOUS: $(DEPDIR)/%.d
.PRECIOUS: $(OBJDIR)/%.o
.PRECIOUS: $(RESULTDIR)/%.txt
.PRECIOUS: $(TSTOBJDIR)/%.o
