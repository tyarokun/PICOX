#include "serial.h"

#define REG32(addr) (*(volatile uint32_t *)(addr))

/* ===== Base addresses ===== */
#define RESETS_BASE      0x4000C000u
#define IO_BANK0_BASE    0x40014000u
#define UART0_BASE       0x40034000u

/* ===== RESETS registers ===== */
#define RESETS_RESET       REG32(RESETS_BASE + 0x00)
#define RESETS_RESET_DONE  REG32(RESETS_BASE + 0x08)

#define RESET_IO_BANK0   (1u << 5)
#define RESET_PADS_BANK0 (1u << 8)
#define RESET_UART0      (1u << 22)

/* ===== IO_BANK0 registers ===== */
#define GPIO_CTRL(gpio) REG32(IO_BANK0_BASE + 0x04 + ((gpio) * 0x08))

#define GPIO_FUNC_UART  2u

/* ===== UART0 registers ===== */
#define UARTDR    REG32(UART0_BASE + 0x00)
#define UARTFR    REG32(UART0_BASE + 0x18)
#define UARTIBRD  REG32(UART0_BASE + 0x24)
#define UARTFBRD  REG32(UART0_BASE + 0x28)
#define UARTLCR_H REG32(UART0_BASE + 0x2C)
#define UARTCR    REG32(UART0_BASE + 0x30)
#define UARTICR   REG32(UART0_BASE + 0x44)

/* ===== UART bits ===== */
#define UARTFR_RXFE   (1u << 4)   /* RX FIFO empty */
#define UARTFR_TXFF   (1u << 5)   /* TX FIFO full */

#define UARTCR_UARTEN (1u << 0)
#define UARTCR_TXE    (1u << 8)
#define UARTCR_RXE    (1u << 9)

#define UARTLCR_H_FEN     (1u << 4)
#define UARTLCR_H_WLEN_8  (3u << 5)

#ifndef UART_CLK_HZ
#define UART_CLK_HZ 125000000u
#endif

#ifndef UART_BAUD
#define UART_BAUD 115200u
#endif

static void uart0_set_baudrate(void)
{
    /*
     * PL011 UART:
     * baud_div = UARTCLK / (16 * baud)
     *
     * IBRD = integer part
     * FBRD = fractional part * 64
     */
    uint32_t baud_div_x64 = ((UART_CLK_HZ * 4u) + (UART_BAUD / 2u)) / UART_BAUD;

    UARTIBRD = baud_div_x64 / 64u;
    UARTFBRD = baud_div_x64 % 64u;
}


// init
void serial_init(void)
{
    /*
     * IO_BANK0, PADS_BANK0, UART0 のリセット解除
     */
    RESETS_RESET &= ~(RESET_IO_BANK0 | RESET_PADS_BANK0 | RESET_UART0);

    while ((RESETS_RESET_DONE & (RESET_IO_BANK0 | RESET_PADS_BANK0 | RESET_UART0))
           != (RESET_IO_BANK0 | RESET_PADS_BANK0 | RESET_UART0)) {
        /* wait */
    }

    /*
     * GP0 = UART0_TX
     * GP1 = UART0_RX
     */
    GPIO_CTRL(0) = GPIO_FUNC_UART;
    GPIO_CTRL(1) = GPIO_FUNC_UART;

    /*
     * 設定中はUARTを無効化
     */
    UARTCR = 0;

    /*
     * 念のため割り込みクリア
     */
    UARTICR = 0x7FF;

    /*
     * 115200 bps, 8N1, FIFO有効
     */
    uart0_set_baudrate();
    UARTLCR_H = UARTLCR_H_WLEN_8 | UARTLCR_H_FEN;

    /*
     * UART有効化、送信・受信有効化
     */
    UARTCR = UARTCR_UARTEN | UARTCR_TXE | UARTCR_RXE;
}


// put
void serial_putc(char c)
{
    if (c == '\n') {
        serial_putc('\r');
    }

    while (UARTFR & UARTFR_TXFF) {
        /* wait */
    }

    UARTDR = (uint32_t)c;
}

void serial_puts(const char *s)
{
    while (*s) {
        serial_putc(*s++);
    }
}

void serial_put_uint(uint32_t value)
{
    char buf[10];
    int index = 0;

    if (value == 0) {
        serial_putc('0');
        return;
    }

    while (value > 0) {
        buf[index++] = '0' + (value % 10);
        value /= 10;
    }

    while (index > 0) {
        serial_putc(buf[--index]);
    }
}

void serial_put_int(int value)
{
    uint32_t magnitude;

    if (value < 0) {
        serial_putc('-');

        /*
         * -2147483648も扱えるように、
         * unsignedに変換してから絶対値を求める。
         */
        magnitude = 0u - (uint32_t)value;
    } else {
        magnitude = (uint32_t)value;
    }

    serial_put_uint(magnitude);
}

void serial_put_hex(uint32_t value)
{
    static const char hex_table[] = "0123456789ABCDEF";
    int shift;

    serial_puts("0x");

    /*
     * 32bit値を必ず8桁で表示する。
     */
    for (shift = 28; shift >= 0; shift -= 4) {
        serial_putc(hex_table[(value >> shift) & 0x0f]);
    }
}

// get
char serial_getc(void)
{
    while (UARTFR & UARTFR_RXFE) {
        /* wait */
    }

    return (char)(UARTDR & 0xFF);
}

int serial_getc_nonblock(void)
{
    if (UARTFR & UARTFR_RXFE) {
        return -1;
    }

    return (int)(UARTDR & 0xFF);
}

void serial_gets(char *buf, int size)
{
    int len = 0;
    char c;

    if (size <= 0) return;
    while (1) {
        c = serial_getc();
        if (c == '\r' || c == '\n') {
            serial_puts("\n");
            break;
        }
        if ((c == '\b' || c == 0x7f) && len > 0) {
            len--;
            serial_puts("\b \b");
        } else if (c >= ' ' && len < size - 1) {
            buf[len++] = c;
            serial_putc(c);
        }
    }
    buf[len] = '\0';
}