MCU=attiny441
F_CPU=8000000UL
TARGET=sl-clock-avr
PROGRAMMER?=usbtiny

EFUSE=ff
HFUSE=df
LFUSE=c2

SOURCES=main.c

ISP_BITCLOCK=1MHz


Q=@

CC=avr-gcc
CFLAGS+=-Wall -Wno-error=unused-function -Wno-error=unused-variable -Os -DF_CPU=$(F_CPU) -mmcu=$(MCU) -std=gnu11 -I. -gdwarf-2 -fverbose-asm -fshort-enums
LDFLAGS+=-Wall -Os -DF_CPU=$(F_CPU) -mmcu=$(MCU) -std=gnu11 -I. -gdwarf-2 -fverbose-asm -fshort-enums -Wl,-Map,$(TARGET).map

#CFLAGS+=-ffunction-sections -fdata-sections
#LDFLAGS+=-Wl,-gc-sections -Wl,-print-gc-sections
#LDFLAGS+=-Wl,-gc-sections

#CFLAGS+=-flto
#LDFLAGS+=-flto=4

OBJCOPY=avr-objcopy
OBJDUMP=avr-objdump
AR=avr-ar
BIN_FORMAT=ihex

PART=$(MCU)

BAUD=115200
PROTOCOL=stk500v1
PART=$(MCU)
AVRDUDE=avrdude

RM=rm -f

CSOURCES=$(filter %.c, $(SOURCES))
ASMSOURCES=$(filter %.asm, $(SOURCES))

OBJECTS=$(CSOURCES:.c=.o) $(ASMSOURCES:.asm=.o)
ASM=$(CSOURCES:.c=.s) $(ASMSOURCES:.asm=.s)

DEPS=$(patsubst %.c, .deps/%.d, $(CSOURCES))

.PHONY: all clean upload fuses fuses_check
.SUFFIXES:

all: $(TARGET).hex

-include $(DEPS)

clean:
	@echo Cleaning up
	$(Q)$(RM) $(OBJECTS) $(ASM) $(TARGET).elf $(TARGET).hex $(DEPS)

upload: $(TARGET).hex
ifeq ($(PROGRAMMER),arduino)
	$(AVRDUDE) -p$(PART) -P$(PORT) -c$(PROGRAMMER) -b$(BAUD) -U flash:w:$<
else
	$(AVRDUDE) -p$(PART) -c$(PROGRAMMER) -B$(ISP_BITCLOCK) -U flash:w:$<
endif

lss: $(TARGET).lss


%.hex: %.elf
	@echo Copying $< to $@
	$(Q)$(OBJCOPY) -O $(BIN_FORMAT) -R .eeprom $< $@

$(TARGET).elf: $(OBJECTS)
	@echo Linking $@
	$(Q)$(CC) $(OBJECTS) $(LDFLAGS) -o $@

$(TARGET).lss: $(TARGET).elf
	@echo Dumping $< to $@
	$(Q)$(OBJDUMP) -h -S $< > $@

%.o: %.c
	@echo Compiling $<
	$(Q)$(CC) $(CFLAGS) -c -o $@ $<
	$(Q)$(CC) -MM $(CFLAGS) $< > .deps/$*.d

%.o: %.asm
	@echo Assembling $<
	$(Q)$(CC) $(CFLAGS) -x assembler-with-cpp -c -o $@ $<

%.o: %.s
	@echo Assembling $<
	$(Q)$(CC) $(CFLAGS) -c -s -o $@ $<

%.s: %.c
	@echo Compiling $<
	$(Q)$(CC) $(CFLAGS) -S -o $@ $<
	$(Q)$(CC) -MM $(CFLAGS) $< > .deps/$*.d

%.s: %.asm
	@echo Pre-processing $<
	$(Q)$(CC) $(CFLAGS) -x assembler-with-cpp -E -o $@ $<

fuses_check:
	@( echo -n "Do you want to write the fuses (L:$(LFUSE) H:$(HFUSE) E:$(EFUSE)) [y/N]? " && read sure && case "$$sure" in [yY]) true;; *) false;; esac )

fuses: fuses_check
	@echo "Writing fuses (L:$(LFUSE) H:$(HFUSE) E:$(EFUSE))"
	$(Q)$(AVRDUDE) -p$(PART) -c$(PROGRAMMER) -U lfuse:w:0x$(LFUSE):m -U hfuse:w:0x$(HFUSE):m -U efuse:w:0x$(EFUSE):m
