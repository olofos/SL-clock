V=@

SOURCES := fonts.c journey.c journey-task.c config.c oled_framebuffer.c matrix_framebuffer.c framebuffer.c oled_display.c matrix_display.c display.c display-message.c \
    json.c json-util.c json-http.c  log.c logo-paw-64x64.c sntp.c sh1106.c timezone-db.c uart.c user_main.c wifi-task.c wifi-list.c wifi-logic.c \
    i2c-master.c http-server-task.c http-server-url-handlers.c syslog.c json-writer.c

TARGET=user

SRCDIR := src
OBJDIR := obj
BINDIR := bin
DEPDIR := .deps
TSTDIR := test/
TSTOBJDIR := test/obj/
TSTBINDIR := test/bin/
TSTDEPDIR := test/.deps/
RESULTDIR := test/results/

BUILD_DIRS = $(OBJDIR) $(DEPDIR) $(BINDIR) $(RESULTDIR) $(TSTOBJDIR) $(TSTBINDIR) $(TSTDEPDIR)

SRC := $(SOURCES:%.c=$(SRCDIR)/%.c)
OBJ := $(SOURCES:%.c=$(OBJDIR)/%.o)
DEPS := $(SOURCES:%.c=$(DEPDIR)/%.d)

SOURCES_TST = $(wildcard $(TSTDIR)*.c)

AR = xtensa-lx106-elf-ar
CC = xtensa-lx106-elf-gcc
NM = xtensa-lx106-elf-nm
OBJCOPY = xtensa-lx106-elf-objcopy
OBJDUMP = xtensa-lx106-elf-objdump

export PATH := $(PATH):$(CURDIR)/esp-open-sdk/xtensa-lx106-elf/bin/
export SDK_PATH := $(CURDIR)/ESP8266_RTOS_SDK/

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
TST_WRAP =
TST_CFLAGS = -Wall -I$(SRCDIR) -I$(BINSRCDIR) -ggdb -fsanitize=address -fno-omit-frame-pointer --coverage $(TST_WRAP)

TST_RESULTS = $(patsubst $(TSTDIR)test_%.c,$(RESULTDIR)test_%.txt,$(SOURCES_TST))
TST_DEPS = $(TSTDEPDIR)*.d

.PHONY: all bin flash clean erase spiffs-flash spiffs-image test build_dirs build-web-app build-sdk

all: build_dirs eagle.app.flash.bin


$(TSTBINDIR)test_oled_framebuffer: $(TSTOBJDIR)oled_framebuffer.o $(TSTOBJDIR)framebuffer.o
$(TSTBINDIR)test_json-writer: $(TSTOBJDIR)json-writer.o
$(TSTBINDIR)test_json-util: $(TSTOBJDIR)json-util.o
$(TSTBINDIR)test_wifi-list: $(TSTOBJDIR)wifi-list.o
$(TSTBINDIR)test_wifi-logic: $(TSTOBJDIR)wifi-logic.o


-include $(DEPS)
-include $(TST_DEPS)

http-sm/lib/libhttp-sm.a:
	$(V)$(MAKE) CC="$(CC)" AR="$(AR)" CFLAGS="$(CFLAGS) -I../src/ -DLOG_SYS=LOG_SYS_HTTP" V=$(V) -Chttp-sm/ lib/libhttp-sm.a

$(OBJDIR)/%.o : $(SRCDIR)/%.c | esp-open-sdk/xtensa-lx106-elf/bin/xtensa-lx106-elf-gcc
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
	$(V)mv eagle.app.v6.text.bin $(BINDIR)/eagle.app.v6.text.bin
	$(V)mv eagle.app.v6.rodata.bin $(BINDIR)/eagle.app.v6.rodata.bin
	$(V)mv eagle.app.v6.data.bin $(BINDIR)/eagle.app.v6.data.bin
	$(V)mv eagle.app.v6.irom0text.bin $(BINDIR)/eagle.app.v6.irom0text.bin
	$(V)mv eagle.app.flash.bin $(BINDIR)/eagle.app.flash.bin

flash: eagle.app.flash.bin
	$(V)esptool.py -p /dev/ttyUSB0 --baud 921600 write_flash -fs 32m -fm dio -ff 40m 0x00000 $(BINDIR)/eagle.app.flash.bin 0x20000 $(BINDIR)/eagle.app.v6.irom0text.bin 0x3fc000 $(SDK_PATH)/$(BINDIR)/esp_init_data_default.bin

mkspiffs/mkspiffs:
	@echo Building mkspiffs
	$(V)$(MAKE) -s -C mkspiffs CPPFLAGS="-DSPIFFS_USE_MAGIC=0 -DSPIFFS_USE_MAGIC_LENGTH=0 -DSPIFFS_OLD_ALIGNMENT=1"

build-web-app:
	@echo Building web app
	$(V)$(MAKE) -s -Cweb-app/

spiffs-image: mkspiffs/mkspiffs build-web-app
	$(V)cp web-app/dist/* data/www/
	@echo Building spiffs image
	$(V)mkspiffs/mkspiffs -b 4096 -p 128 -s 196608 -c data/ $(BINDIR)/spiffs.bin

spiffs-flash: spiffs-image
	$(V)esptool.py  -p /dev/ttyUSB0 --baud 921600 write_flash -fs 32m 0x100000 $(BINDIR)/spiffs.bin

erase:
	$(V)esptool.py -p /dev/ttyUSB0 --baud 921600 erase_flash

esp-open-sdk/xtensa-lx106-elf/bin/xtensa-lx106-elf-gcc:
	$(V)$(MAKE) build-sdk -s

build-sdk:
	@echo Building toolchain
	$(V)cd esp-open-sdk; $(MAKE) STANDALONE=n -s


test: build_dirs $(TST_RESULTS)
	@echo "-----------------------"
	@echo "SKIPPED:" `grep -o '\[  SKIPPED \]' $(RESULTDIR)*.txt|wc -l`
	@echo "-----------------------"
	@grep -s '\[  SKIPPED \]' $(RESULTDIR)*.txt || true
	@echo "\n-----------------------"
	@echo "FAILED:" `grep -o '\[  FAILED  \]' $(RESULTDIR)*.txt|wc -l`
	@echo "-----------------------"
	@grep -s 'FAILED\|LINE\|ERROR' $(RESULTDIR)*.txt || true
	@echo "\n-----------------------"
	@echo "PASSED:" `grep -o '\[       OK \]' $(RESULTDIR)*.txt|wc -l`
	@echo "-----------------------"
	@echo
	@! grep -s '\[  FAILED  \]' $(RESULTDIR)*.txt 2>&1 1>/dev/null

build_dirs:
	$(V)mkdir -p $(BUILD_DIRS)


$(RESULTDIR)%.txt: $(TSTBINDIR)%
	@echo Running $<
	@echo
	$(V)./$< > $@ 2>&1 || true

$(TSTOBJDIR)%.o : $(TSTDIR)%.c
	@echo CC $@
	$(V)$(TST_CC) $(TST_CFLAGS)  $(INCLUDES) -c $< -o $@
	$(V)$(TST_CC) -MM -MT $@ $(TST_CFLAGS) $(INCLUDES) $< > $(TSTDEPDIR)$*.d

$(TSTOBJDIR)%.o : $(SRCDIR)/%.c
	@echo CC $@
	$(V)$(TST_CC) $(TST_CFLAGS)  $(INCLUDES) -c $< -o $@
	$(V)$(TST_CC) -MM -MT $@ $(TST_CFLAGS) $(INCLUDES) $< > $(TSTDEPDIR)$*.d

$(TSTBINDIR)test_%: $(TSTOBJDIR)test_%.o
	@echo CC $@
	$(V)$(TST_CC) -o $@ $(TST_CFLAGS) $^ -lcmocka

clean:
	@echo Cleaning
	$(V)-rm -f $(OBJ) $(OBJDIR)/libuser.a $(OBJDIR)/user.elf $(TSTOBJDIR)/*.o $(TSTBINDIR)/test_* $(RESULTDIR)/*.txt $(DEPDIR)/*.d $(BINDIR)/eagle.app.v6.text.bin $(BINDIR)/eagle.app.v6.rodata.bin $(BINDIR)/eagle.app.v6.data.bin $(BINDIR)/eagle.app.v6.irom0text.bin $(BINDIR)/eagle.app.flash.bin
	$(V)$(MAKE) -Chttp-sm clean

.PRECIOUS: $(TSTBINDIR)/test_%
.PRECIOUS: $(DEPDIR)/%.d
.PRECIOUS: $(OBJDIR)/%.o
.PRECIOUS: $(RESULTDIR)/%.txt
.PRECIOUS: $(TSTOBJDIR)/%.o
