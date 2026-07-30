TARGET := PICOX

CC := arm-none-eabi-gcc
GDB := arm-none-eabi-gdb
OPENOCD := openocd

SRC_DIR := src
BUILD_DIR := build

OPENOCD_INTERFACE := interface/cmsis-dap.cfg
OPENOCD_TARGET := target/rp2040.cfg
ADAPTER_SPEED := 1000

CFLAGS := -mcpu=cortex-m0plus -mthumb -std=c11 -Wall -Wextra \
          -ffreestanding -fno-builtin -O0 -g3 \
          -I$(SRC_DIR)

LDFLAGS := -T $(SRC_DIR)/linker.ld -nostartfiles -nostdlib \
           -Wl,-Map=$(BUILD_DIR)/$(TARGET).map

OBJS := boot2.o handler.o vector_table.o serial.o main.o clock.o interrupt.o kernel.o syscall.o lib.o memory.o shell.o consdrv.o exec.o sddrv.o spi.o elf_loader.o fat32.o app_memory.o

OBJS := $(addprefix $(BUILD_DIR)/,$(OBJS))

.PHONY: all flash openocd gdb clean reset erase

all: $(BUILD_DIR)/$(TARGET).elf

$(BUILD_DIR)/$(TARGET).elf: $(OBJS)
	$(CC) $(CFLAGS) \
	      $(LDFLAGS) \
	      -o $(BUILD_DIR)/$(TARGET).elf \
	      $(OBJS) \
	      -lgcc

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -c -o $@ $<

flash: $(BUILD_DIR)/$(TARGET).elf
	$(OPENOCD) -f $(OPENOCD_INTERFACE) \
	          -f $(OPENOCD_TARGET) \
	          -c "adapter speed $(ADAPTER_SPEED)" \
	          -c "program $(BUILD_DIR)/$(TARGET).elf verify reset exit"

openocd:
	$(OPENOCD) -f $(OPENOCD_INTERFACE) \
	          -f $(OPENOCD_TARGET) \
	          -c "adapter speed $(ADAPTER_SPEED)"

gdb: $(BUILD_DIR)/$(TARGET).elf
	$(GDB) $(BUILD_DIR)/$(TARGET).elf

clean:
	rm -rf $(BUILD_DIR)

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