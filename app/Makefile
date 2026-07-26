TARGET := led.elf

CC := arm-none-eabi-gcc

CFLAGS := \
	-mcpu=cortex-m0plus \
	-mthumb \
	-std=c11 \
	-Wall \
	-Wextra \
	-Os \
	-ffreestanding \
	-fno-builtin \
	-fno-pic \
	-fno-pie \
	-fno-unwind-tables \
	-fno-asynchronous-unwind-tables

LDFLAGS := \
	-nostartfiles \
	-nostdlib \
	-Wl,--build-id=none \
	-Wl,-N \
	-Wl,-e,main \
	-Wl,-Ttext=0x20020000

all: $(TARGET)

$(TARGET): led.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

led.o: led.c
	$(CC) $(CFLAGS) -c -o $@ $<

inspect: $(TARGET)
	arm-none-eabi-readelf -h -l $(TARGET)

clean:
	rm -f led.o $(TARGET)

.PHONY: all inspect clean
