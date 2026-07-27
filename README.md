# PICOX

PICOX is an educational embedded operating system for the Raspberry Pi Pico
(RP2040 / Arm Cortex-M0+).

## Pin assignment

### UART0

| Signal | GPIO | Pico pin |
|---|---:|---:|
| UART0 TX | GPIO0 | 1 |
| UART0 RX | GPIO1 | 2 |
| GND | - | 3 |

Connect the UART adapter as follows:

```text
Pico GPIO0 (TX) -> USB-UART RX
Pico GPIO1 (RX) -> USB-UART TX
Pico GND        -> USB-UART GND
```

Use a 3.3 V UART adapter.

### SD card / SPI0

| Signal | GPIO | Pico pin | SD card |
|---|---:|---:|---|
| MISO | GPIO16 | 21 | DO |
| CS | GPIO17 | 22 | CS |
| SCK | GPIO18 | 24 | CLK |
| MOSI | GPIO19 | 25 | DI |
| 3.3 V | - | 36 | VCC |
| GND | - | 23 or 38 | GND |

```text
Pico GPIO16 (MISO) -> SD DO
Pico GPIO17 (CS)   -> SD CS
Pico GPIO18 (SCK)  -> SD CLK
Pico GPIO19 (MOSI) -> SD DI
Pico 3.3V          -> SD VCC
Pico GND           -> SD GND
```

Use an SD card module that supports 3.3 V logic.

## Requirements

- Raspberry Pi Pico
- Raspberry Pi Pico SDK
- GNU Arm Embedded Toolchain
- CMake
- Make
- USB-UART adapter
- SPI microSD card module

## Build

Set `PICO_SDK_PATH`:

```sh
export PICO_SDK_PATH=/path/to/pico-sdk
```

Create the build directory and build PICOX:

```sh
mkdir -p build
cd build
cmake ..
make
```

After a successful build, write the generated UF2 file to the Raspberry Pi
Pico while it is in BOOTSEL mode.

## Starting PICOX

Connect the USB-UART adapter and open a serial terminal.

Example:

```sh
screen /dev/tty.usbserial-XXXX 115200
```

After startup:

```text
PICOX shell started
picox>
```

## Shell commands

The available commands include:

```text
help
echo
version
clear
irqstat
reset
run FILE.ELF
```

Show the command list:

```text
picox> help
```

Run an ELF application from the SD card:

```text
picox> run LED.ELF
```

The ELF filename should use a FAT 8.3-compatible name.

Example:

```text
LED.ELF
APP.ELF
TEST.ELF
```

## SD card preparation

1. Format the SD card as FAT32.
2. Copy an ELF application to the root directory.
3. Insert the SD card before starting PICOX.
4. Start PICOX.
5. Execute the file with the `run` command.

Example:

```text
picox> run LED.ELF
```

PICOX reads the ELF file from the SD card, loads its loadable segments into the
application RAM area, and calls its entry point.

## Application memory area

The current ELF loader uses the following RAM area:

```text
0x20020000 - 0x2003FFFF
```

Applications must be linked so that their loadable segments and entry point are
inside this range.
