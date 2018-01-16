SOURCES := brzo_i2c.c fonts.c http-client.c icon-boat-large.c icon-bus-large.c icon-clock-large.c icon-noclock-large.c icon-nowifi1-large.c icon-nowifi2-large.c icon-nowifi3-large.c icon-nowifi4-large.c journey.c journey-task.c\
    json.c json-util.c log.c logo-paw-64x64.c sntp.c ssd1306.c timezone-db.c uart.c user_main.c wifi-task.c

TARGET=user

SRCDIR := src
OBJDIR := obj

SRC := $(SOURCES:%.c=$(SRCDIR)/%.c)
OBJ := $(SOURCES:%.c=$(OBJDIR)/%.o)

AR = xtensa-lx106-elf-ar
CC = xtensa-lx106-elf-gcc
NM = xtensa-lx106-elf-nm
OBJCOPY = xtensa-lx106-elf-objcopy
OBJDUMP = xtensa-lx106-elf-objdump


CFLAGS = -DPROGMEM= -std=gnu99 -Os -g -Wpointer-arith -Wundef -Wall -Wl,-EL -fno-inline-functions -nostdlib -mlongcalls -mtext-section-literals \
         -ffunction-sections -fdata-sections -fno-builtin-printf -fno-jump-tables $(INCLUDES)

LDFILE = ld/eagle.app.v6.ld

LDFLAGS = -L$(SDK_PATH)/lib -Wl,--gc-sections -nostdlib -T$(LDFILE) -Wl,--no-check-sections -u call_user_start -Wl,-static -Wl,--start-group -lcirom -lcrypto -lespconn -lespnow -lfreertos -lgcc -lhal -llwip -lmain -lmirom -lnet80211 -lnopoll -lphy -lpp -lpwm -lsmartconfig -lspiffs -lssl -lwpa obj/libuser.a -lwps -Wl,--end-group

INCLUDES := $(INCLUDES)
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

.PHONY: all bin flash clean

all: eagle.app.flash.bin

$(OBJDIR)/%.o : $(SRCDIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/libuser.a: $(OBJ)
	$(AR) ru $@ $^

$(OBJDIR)/user.elf : $(OBJDIR)/libuser.a
	$(CC) $(LDFLAGS) -o $@

eagle.app.flash.bin: $(OBJDIR)/user.elf
	$(OBJCOPY) --only-section .text -O binary obj/user.elf eagle.app.v6.text.bin
	$(OBJCOPY) --only-section .rodata -O binary obj/user.elf eagle.app.v6.rodata.bin
	$(OBJCOPY) --only-section .data -O binary obj/user.elf eagle.app.v6.data.bin
	$(OBJCOPY) --only-section .irom0.text -O binary obj/user.elf eagle.app.v6.irom0text.bin
	python $(SDK_PATH)/tools/gen_appbin.py obj/user.elf 0 2 0 4
	mv eagle.app.v6.text.bin bin/eagle.app.v6.text.bin
	mv eagle.app.v6.rodata.bin bin/eagle.app.v6.rodata.bin
	mv eagle.app.v6.data.bin bin/eagle.app.v6.data.bin
	mv eagle.app.v6.irom0text.bin bin/eagle.app.v6.irom0text.bin
	mv eagle.app.flash.bin bin/eagle.app.flash.bin

flash: eagle.app.flash.bin
	esptool.py -p /dev/ttyUSB0 --baud 921600 write_flash -fs 32m -fm dio -ff 40m 0x00000 bin/eagle.app.flash.bin 0x20000 bin/eagle.app.v6.irom0text.bin

clean:
	rm -f $(OBJ) $(OBJDIR)/libuser.a $(OBJDIR)/user.elf bin/eagle.app.v6.text.bin bin/eagle.app.v6.rodata.bin bin/eagle.app.v6.data.bin bin/eagle.app.v6.irom0text.bin bin/eagle.app.flash.bin
