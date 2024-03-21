LD= /opt/gcc-cross-mipsel/8.2.0/bin/mipsel-unknown-elf-ld
CC= /opt/gcc-cross-mipsel/8.2.0/bin/mipsel-unknown-elf-gcc
AS= /opt/gcc-cross-mipsel/8.2.0/bin/mipsel-unknown-elf-as
DU= /opt/gcc-cross-mipsel/8.2.0/bin/mipsel-unknown-elf-objdump

SYS_OBJS=  reset.o \
	   giet.o \
	   drivers.o \
	   common.o \
           ctx_handler.o \
           sys_handler.o \
           irq_handler.o \
           exc_handler.o 

APP_OBJS=  stdio.o \
	   main_dma.o 

GIET_SYS_PATH= /users/enseig/alain/giet_2011/sys
GIET_APP_PATH= /users/enseig/alain/giet_2011/app

CFLAGS= -Wall -mno-gpopt -ffreestanding -mips32 

## system & application compilation

all: sys.bin app.bin

## system compilation

sys.bin: $(SYS_OBJS) sys.ld seg.ld
	$(LD) -o $@ -T sys.ld $(SYS_OBJS)
	$(DU) -D $@ > $@.txt

giet.o: $(GIET_SYS_PATH)/giet.s
	$(AS) -g -mips32 -o $@ $<
	$(DU) -D $@ > $@.txt

reset.o: reset.s
	$(AS) -g -mips32 -o $@ $<
	$(DU) -D $@ > $@.txt

drivers.o: $(GIET_SYS_PATH)/drivers.c $(GIET_SYS_PATH)/drivers.h config.h
	$(CC) $(CFLAGS) -I$(GIET_SYS_PATH) -I. -c -o $@ $<
	$(DU) -D $@ > $@.txt

common.o: $(GIET_SYS_PATH)/common.c $(GIET_SYS_PATH)/common.h config.h
	$(CC) $(CFLAGS) -I$(GIET_SYS_PATH) -I. -c -o $@ $<
	$(DU) -D $@ > $@.txt

ctx_handler.o: $(GIET_SYS_PATH)/ctx_handler.c $(GIET_SYS_PATH)/ctx_handler.h config.h
	$(CC) $(CFLAGS) -I$(GIET_SYS_PATH) -I. -c -o $@ $<
	$(DU) -D $@ > $@.txt

sys_handler.o: $(GIET_SYS_PATH)/sys_handler.c $(GIET_SYS_PATH)/sys_handler.h config.h
	$(CC) $(CFLAGS) -I$(GIET_SYS_PATH) -I. -c -o $@ $<
	$(DU) -D $@ > $@.txt

irq_handler.o: $(GIET_SYS_PATH)/irq_handler.c $(GIET_SYS_PATH)/irq_handler.h config.h
	$(CC) $(CFLAGS) -I$(GIET_SYS_PATH) -I. -c -o $@ $<
	$(DU) -D $@ > $@.txt

exc_handler.o: $(GIET_SYS_PATH)/exc_handler.c $(GIET_SYS_PATH)/exc_handler.h config.h
	$(CC) $(CFLAGS) -I$(GIET_SYS_PATH) -I. -c -o $@ $<
	$(DU) -D $@ > $@.txt

## application compilation  

app.bin: $(APP_OBJS) app.ld seg.ld
	$(LD) -o $@ -T app.ld $(APP_OBJS)
	$(DU) -D $@ > $@.txt

stdio.o: $(GIET_APP_PATH)/stdio.c $(GIET_APP_PATH)/stdio.h
	$(CC) $(CFLAGS) -I$(GIET_APP_PATH) -I. -c -o $@ $<
	$(DU) -D $@ > $@.txt

main_dma.o: main_pipe4.c 
	$(CC) $(CFLAGS) -I$(GIET_APP_PATH) -I. -c -o $@ $<
	$(DU) -D $@ > $@.txt



clean:
	rm -f *.o *.bin *.txt core *~
