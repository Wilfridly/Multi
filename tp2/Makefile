GIET_APP_PATH = /giet2011/app //A COMPLETER

all: sys.bin app.bin

sys.bin:
    $LD -o sys.bin -T sys.ld reset.o giet.o drivers.o common.o ctx_handler.o irq_handler.o sys_handler.o exc_handler.o

app.bin:
    $CC -Wall -mno-gpopt -ffreestanding -mips32 -I$(GIET_APP_PATH) -I. -c -o stdio.o $(GIET_APP_PATH)/stdio.c
    $CC -Wall -mno-gpopt -ffreestanding -mips32 -I$(GIET_APP_PATH) -I. -c -o main.o main.c
    $LD -o app.bin -T app.ld stdio.o main.o

clean:
    rm -f *.o sys.bin app.bin

.PHONY: all clean