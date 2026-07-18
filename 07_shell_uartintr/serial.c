#include "serial.h"

#define REG32(addr) (*(volatile uint32_t *)(addr))

/* ===== Base addresses ===== */
#define RESETS_BASE      0x4000C000u
#define IO_BANK0_BASE    0x40014000u
#define UART0_BASE       0x40034000u
#define NVIC_ISER_BASE   0xE000E100u
#define NVIC_ICER_BASE   0xE000E180u
#define NVIC_ICPR_BASE   0xE000E280u

/* RP2040ではUART0_IRQは外部IRQ番号20。 */
#define UART0_IRQ_NUM    20u
#define UART0_IRQ_MASK   (1u << UART0_IRQ_NUM)

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
#define UARTDR       REG32(UART0_BASE + 0x00)
#define UARTRSR_ECR  REG32(UART0_BASE + 0x04)
#define UARTFR       REG32(UART0_BASE + 0x18)
#define UARTIBRD     REG32(UART0_BASE + 0x24)
#define UARTFBRD     REG32(UART0_BASE + 0x28)
#define UARTLCR_H    REG32(UART0_BASE + 0x2C)
#define UARTCR       REG32(UART0_BASE + 0x30)
#define UARTIFLS     REG32(UART0_BASE + 0x34)
#define UARTIMSC     REG32(UART0_BASE + 0x38)
#define UARTMIS      REG32(UART0_BASE + 0x40)
#define UARTICR      REG32(UART0_BASE + 0x44)

/* ===== UART bits ===== */
#define UARTDR_ERROR_MASK (0x0fu << 8)

#define UARTFR_RXFE   (1u << 4)
#define UARTFR_TXFF   (1u << 5)

#define UARTCR_UARTEN (1u << 0)
#define UARTCR_TXE    (1u << 8)
#define UARTCR_RXE    (1u << 9)

#define UARTLCR_H_FEN     (1u << 4)
#define UARTLCR_H_WLEN_8  (3u << 5)

#define UARTIFLS_RX_MASK  (7u << 3)
#define UARTIFLS_RX_1_8   (0u << 3)

#define UARTINT_RX        (1u << 4)
#define UARTINT_RT        (1u << 6)
#define UARTINT_FE        (1u << 7)
#define UARTINT_PE        (1u << 8)
#define UARTINT_BE        (1u << 9)
#define UARTINT_OE        (1u << 10)
#define UARTINT_RX_ERRORS (UARTINT_FE | UARTINT_PE | UARTINT_BE | UARTINT_OE)
#define UARTINT_RX_ALL    (UARTINT_RX | UARTINT_RT | UARTINT_RX_ERRORS)

#ifndef UART_CLK_HZ
#define UART_CLK_HZ 125000000u
#endif

#ifndef UART_BAUD
#define UART_BAUD 115200u
#endif

#define SERIAL_RX_BUFFER_SIZE 128u
#define SERIAL_RX_BUFFER_MASK (SERIAL_RX_BUFFER_SIZE - 1u)

#if (SERIAL_RX_BUFFER_SIZE & SERIAL_RX_BUFFER_MASK) != 0
#error SERIAL_RX_BUFFER_SIZE must be a power of two
#endif

/*
 * ISRだけがheadを書き、シェルタスクだけがtailを書く。
 * 1 producer / 1 consumerなので、通常処理側で割り込み禁止にする必要はない。
 */
static volatile uint8_t rx_buffer[SERIAL_RX_BUFFER_SIZE];
static volatile uint32_t rx_head;
static volatile uint32_t rx_tail;
static volatile uint32_t rx_overflow;

static void uart0_set_baudrate(void)
{
    uint32_t baud_div_x64;

    /* baud_div = UARTCLK / (16 * baud) */
    baud_div_x64 = ((UART_CLK_HZ * 4u) + (UART_BAUD / 2u)) / UART_BAUD;
    UARTIBRD = baud_div_x64 / 64u;
    UARTFBRD = baud_div_x64 % 64u;
}

static int rx_buffer_push(uint8_t value)
{
    uint32_t head = rx_head;
    uint32_t next = (head + 1u) & SERIAL_RX_BUFFER_MASK;

    if (next == rx_tail) {
        rx_overflow++;
        return -1;
    }

    rx_buffer[head] = value;
    rx_head = next;
    return 0;
}

void serial_init(void)
{
    RESETS_RESET &= ~(RESET_IO_BANK0 | RESET_PADS_BANK0 | RESET_UART0);

    while ((RESETS_RESET_DONE &
            (RESET_IO_BANK0 | RESET_PADS_BANK0 | RESET_UART0)) !=
           (RESET_IO_BANK0 | RESET_PADS_BANK0 | RESET_UART0)) {
        /* wait */
    }

    /* GP0 = UART0_TX, GP1 = UART0_RX */
    GPIO_CTRL(0) = GPIO_FUNC_UART;
    GPIO_CTRL(1) = GPIO_FUNC_UART;

    UARTCR = 0;
    UARTIMSC = 0;
    UARTICR = 0x7ffu;
    UARTRSR_ECR = 0;

    rx_head = 0;
    rx_tail = 0;
    rx_overflow = 0;

    uart0_set_baudrate();
    UARTLCR_H = UARTLCR_H_WLEN_8 | UARTLCR_H_FEN;

    /* RX FIFO割り込みのしきい値を1/8にする。 */
    UARTIFLS = (UARTIFLS & ~UARTIFLS_RX_MASK) | UARTIFLS_RX_1_8;

    UARTCR = UARTCR_UARTEN | UARTCR_TXE | UARTCR_RXE;
}

void serial_enable_rx_interrupt(void)
{
    /* 以前の保留状態を消してからUART側とNVIC側を有効にする。 */
    UARTICR = UARTINT_RX_ALL;
    REG32(NVIC_ICPR_BASE) = UART0_IRQ_MASK;

    UARTIMSC |= UARTINT_RX_ALL;
    REG32(NVIC_ISER_BASE) = UART0_IRQ_MASK;

    __asm volatile ("dsb");
    __asm volatile ("isb");
}

void serial_disable_rx_interrupt(void)
{
    REG32(NVIC_ICER_BASE) = UART0_IRQ_MASK;
    UARTIMSC &= ~UARTINT_RX_ALL;
    UARTICR = UARTINT_RX_ALL;
}

/*
 * UART0割り込みの上位処理。
 * ハードウェアFIFOを空になるまで読み、OS側リングバッファへ移す。
 */
int serial_rx_interrupt_handler(void)
{
    uint32_t status = UARTMIS;
    int stored = 0;

    while (!(UARTFR & UARTFR_RXFE)) {
        uint32_t data = UARTDR;

        if (data & UARTDR_ERROR_MASK) {
            UARTRSR_ECR = 0;
            continue;
        }

        if (rx_buffer_push((uint8_t)data) == 0) {
            stored++;
        }
    }

    /* RX/timeout/errorの各要因をクリアする。 */
    UARTICR = status & UARTINT_RX_ALL;
    return stored;
}

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
        buf[index++] = (char)('0' + (value % 10u));
        value /= 10u;
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
    for (shift = 28; shift >= 0; shift -= 4) {
        serial_putc(hex_table[(value >> shift) & 0x0fu]);
    }
}

char serial_getc(void)
{
    while (UARTFR & UARTFR_RXFE) {
        /* wait */
    }

    return (char)(UARTDR & 0xffu);
}

int serial_getc_nonblock(void)
{
    if (UARTFR & UARTFR_RXFE) {
        return -1;
    }

    return (int)(UARTDR & 0xffu);
}

int serial_readc_nonblock(void)
{
    uint32_t tail = rx_tail;

    if (tail == rx_head) {
        return -1;
    }

    rx_tail = (tail + 1u) & SERIAL_RX_BUFFER_MASK;
    return (int)rx_buffer[tail];
}

uint32_t serial_rx_overflow_count(void)
{
    return rx_overflow;
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