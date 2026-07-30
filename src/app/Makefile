# ディレクトリ内のすべての .c ファイルを取得 (例: led.c red.c)
SRCS := $(wildcard *.c)

# .c から対応する .elf のリストを生成 (例: led.elf red.elf)
TARGETS := $(patsubst %.c,%.elf,$(SRCS))

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

# すべての .elf ファイルをビルド対象にする
all: $(TARGETS)

# パターンルール: %.elf は対応する %.o から作成
%.elf: %.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $<

# パターンルール: %.o は対応する %.c から作成
%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

inspect: $(TARGETS)
	arm-none-eabi-readelf -h -l $(TARGETS)

clean:
	rm -f *.o *.elf

.PHONY: all inspect clean