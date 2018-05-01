DEVICE = attiny85
F_CPU = 16500000

CFLAGS = -Iusbdrv -Ilibs-device -I.
AVRCC = avr-gcc -Wall -Os -DF_CPU=$(F_CPU) $(CFLAGS) -mmcu=$(DEVICE)

OBJECTS = usbdrv/usbdrv.o usbdrv/usbdrvasm.o usbdrv/oddebug.o \
	libs-device/osccalASM.o main.o

all: main.hex

main.elf: $(OBJECTS) usbconfig.h
	$(AVRCC) -o main.elf $(OBJECTS)

main.hex: main.elf
	avr-objcopy -j .text -j .data -O ihex main.elf main.hex
	avr-size main.hex

clean:
	rm -f $(OBJECTS) main.elf main.hex

# Build rules to use AVRCC rather than the host CC

.c.o:
	$(AVRCC) -c $< -o $@

.S.o:
	$(AVRCC) -x assembler-with-cpp -c $< -o $@

# Hard coded dependencies for the usbdrv bits; otherwise changes to
# usbconfig.h don't result in enough bits getting recompiled.

libs-device/osccalASM.o: libs-device/osccalASM.S usbdrv/usbdrv.h usbconfig.h \
 libs-device/osccal.h usbdrv/usbportability.h

usbdrv/oddebug.o: usbdrv/oddebug.c usbdrv/oddebug.h usbdrv/usbportability.h

usbdrv/usbdrv.o: usbdrv/usbdrv.c usbdrv/usbdrv.h usbconfig.h \
 libs-device/osccal.h usbdrv/usbportability.h usbdrv/oddebug.h

usbdrv/usbdrvasm.o: usbdrv/usbdrvasm.S usbdrv/usbportability.h \
 usbdrv/usbdrv.h usbconfig.h libs-device/osccal.h usbdrv/usbdrvasm165.inc \
 usbdrv/asmcommon.inc
