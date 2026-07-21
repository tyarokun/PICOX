TARGET := PICOX

CC := arm-none-eabi-gcc
GDB := arm-none-eabi-gdb
OPENOCD := openocd

OPENOCD_INTERFACE := interface/cmsis-dap.cfg
OPENOCD_TARGET := target/rp2040.cfg
ADAPTER_SPEED := 1000

CFLAGS := -mcpu=cortex-m0plus -mthumb -std=c11 -Wall -Wextra \
          -ffreestanding -fno-builtin -O0 -g3

LDFLAGS := -T linker.ld -nostartfiles -nostdlib \
           -Wl,-Map=$(TARGET).map

OBJS := boot2.o handler.o vector_table.o serial.o main.o clock.o interrupt.o kernel.o syscall.o lib.o memory.o command.o consdrv.o

.PHONY: all flash openocd gdb clean reset erase

all: $(TARGET).elf

$(TARGET).elf: $(OBJS)
	$(CC) $(CFLAGS) \
	      $(LDFLAGS) \
	      -o $(TARGET).elf \
	      $(OBJS) \
	      -lgcc

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

flash: $(TARGET).elf
	$(OPENOCD) -f $(OPENOCD_INTERFACE) \
	          -f $(OPENOCD_TARGET) \
	          -c "adapter speed $(ADAPTER_SPEED)" \
	          -c "program $(TARGET).elf verify reset exit"

openocd:
	$(OPENOCD) -f $(OPENOCD_INTERFACE) \
	          -f $(OPENOCD_TARGET) \
	          -c "adapter speed $(ADAPTER_SPEED)"

gdb: $(TARGET).elf
	$(GDB) $(TARGET).elf

clean:
	rm -f *.o $(TARGET).elf $(TARGET).map $(TARGET).bin

reset:
	$(OPENOCD) -f $(OPENOCD_INTERFACE) \
	          -f $(OPENOCD_TARGET) \
	          -c "adapter speed $(ADAPTER_SPEED)" \
	          -c "init; reset run; shutdown"

erase:
	$(OPENOCD) -f $(OPENOCD_INTERFACE) \
	          -f $(OPENOCD_TARGET) \
	          -c "adapter speed $(ADAPTER_SPEED)" \
	          -c "init; reset halt; flash erase_address 0x10000000 0x200000; reset run; shutdown"