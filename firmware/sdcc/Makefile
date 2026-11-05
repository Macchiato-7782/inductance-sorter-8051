SDCC ?= sdcc
PACKIHX ?= packihx
CFLAGS ?= -mmcs51 --model-small --std-c99
TARGET = exp6_8051

all: $(TARGET).hex

main_sdcc.rel: main_sdcc.c
	$(SDCC) $(CFLAGS) -c main_sdcc.c

main_sdcc.ihx: main_sdcc.rel
	$(SDCC) $(CFLAGS) main_sdcc.rel

$(TARGET).hex: main_sdcc.ihx
	$(PACKIHX) main_sdcc.ihx > $(TARGET).hex

clean:
	rm -f *.rel *.ihx *.hex *.asm *.lst *.sym *.map *.mem *.lk *.rst

.PHONY: all clean
