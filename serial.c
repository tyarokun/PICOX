#include "serial.h"

#define REG32(addr) (*(volatile uint32_t *)(addr))

/* Base addresses */
#define RESETS_BASE      0x4000C000u
#define IO_BANK0_BASE    0x40014000u
#define UART0_BASE       0x40034000u
#define NVIC_ISER_BASE   0xE000E100u
#define NVIC_ICER_BASE   0xE000E180u
#define NVIC_ICPR_BASE   0xE000E280u

/* RP2040 UART0 external interrupt number */
#define UART0_IRQ_NUM    20u
#define UART0_IRQ_MASK   (1u << UART0_IRQ_NUM)

/* RESETS */
#define RESETS_RESET       REG32(RESETS_BASE + 0x00)
#define RESETS_RESET_DONE  REG32(RESETS_BASE + 0x08)

#define RESET_IO_BANK0   (1u << 5)
#define RESET_PADS_BANK0 (1u << 8)
#define RESET_UART0      (1u << 22)

/* IO_BANK0 */
#define GPIO_CTRL(gpio) REG32(IO_BANK0_BASE + 0x04 + ((gpio) * 0x08))
#define GPIO_FUNC_UART  2u

/* UART0: ARM PL011-compatible register block */
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

/* UART data/status bits */
#define UARTDR_ERROR_MASK (0x0fu << 8)

#define UARTFR_RXFE   (1u << 4)
#define UARTFR_TXFF   (1u << 5)

#define UARTCR_UARTEN (1u << 0)
#define UARTCR_TXE    (1u << 8)
#define UARTCR_RXE    (1u << 9)

#define UARTLCR_H_FEN     (1u << 4)
#define UARTLCR_H_WLEN_8  (3u << 5)

#define UARTIFLS_TX_MASK  (7u << 0)
#define UARTIFLS_RX_MASK  (7u << 3)
#define UARTIFLS_TX_1_8   (0u << 0)
#define UARTIFLS_RX_1_8   (0u << 3)

#define UARTINT_RX        (1u << 4)
#define UARTINT_TX        (1u << 5)
#define UARTINT_RT        (1u << 6)
#define UARTINT_FE        (1u << 7)
#define UARTINT_PE        (1u << 8)
#define UARTINT_BE        (1u << 9)
#define UARTINT_OE        (1u << 10)

#define UARTINT_RX_ERRORS (UARTINT_FE | UARTINT_PE | UARTINT_BE | UARTINT_OE)
// 受信タイムアウト割り込みRTを有効化 → 32 ÷ 115200 ≒ 0.000278秒 ≒ 278マイクロ秒
#define UARTINT_RX_ALL    (UARTINT_RX | UARTINT_RT | UARTINT_RX_ERRORS)

#ifndef UART_CLK_HZ
#define UART_CLK_HZ 125000000u
#endif

#ifndef UART_BAUD
#define UART_BAUD 115200u
#endif

static void uart0_set_baudrate(void){
    uint32_t baud_div_x64;

    /*
     * baud_div = UARTCLK / (16 * baud)
     * The PL011 fractional divider stores baud_div * 64.
     */
    baud_div_x64 = ((UART_CLK_HZ * 4u) + (UART_BAUD / 2u)) / UART_BAUD;

    UARTIBRD = baud_div_x64 / 64u;
    UARTFBRD = baud_div_x64 % 64u;
}

void serial_init(void){
    RESETS_RESET &= ~(RESET_IO_BANK0 | RESET_PADS_BANK0 | RESET_UART0);

    while ((RESETS_RESET_DONE & (RESET_IO_BANK0 | RESET_PADS_BANK0 | RESET_UART0)) != (RESET_IO_BANK0 | RESET_PADS_BANK0 | RESET_UART0)) {
        /* wait for reset release */
    }

    /* GP0 = UART0_TX, GP1 = UART0_RX */
    GPIO_CTRL(0) = GPIO_FUNC_UART;
    GPIO_CTRL(1) = GPIO_FUNC_UART;

    /* Disable UART and all UART/NVIC interrupts while configuring. */
    UARTCR = 0;
    UARTIMSC = 0;
    UARTICR = 0x7ffu;
    UARTRSR_ECR = 0;

    REG32(NVIC_ICER_BASE) = UART0_IRQ_MASK;
    REG32(NVIC_ICPR_BASE) = UART0_IRQ_MASK;

    uart0_set_baudrate();

    UARTLCR_H = UARTLCR_H_WLEN_8 | UARTLCR_H_FEN;

    //RP2040のUART受信FIFOは32文字分あり、現在の受信割り込みしきい値は 1/8 に設定 → 32 / 8 = 4
    UARTIFLS = (UARTIFLS & ~(UARTIFLS_TX_MASK | UARTIFLS_RX_MASK)) | UARTIFLS_TX_1_8 | UARTIFLS_RX_1_8;

    UARTCR = UARTCR_UARTEN | UARTCR_TXE | UARTCR_RXE;
}

int serial_is_send_enable(void){
    return (UARTFR & UARTFR_TXFF) == 0;
}

void serial_send_byte(uint8_t value){
    UARTDR = (uint32_t)value;
}

int serial_is_recv_enable(void){
    return (UARTFR & UARTFR_RXFE) == 0;
}

uint8_t serial_recv_byte(void){
    uint32_t data = UARTDR;

    if (data & UARTDR_ERROR_MASK) {
        UARTRSR_ECR = 0;
    }

    return (uint8_t)data;
}

void serial_interrupt_enable(void){
    REG32(NVIC_ICPR_BASE) = UART0_IRQ_MASK;
    REG32(NVIC_ISER_BASE) = UART0_IRQ_MASK;

    __asm volatile ("dsb");
    __asm volatile ("isb");
}

void serial_interrupt_disable(void){
    REG32(NVIC_ICER_BASE) = UART0_IRQ_MASK;
    REG32(NVIC_ICPR_BASE) = UART0_IRQ_MASK;
}

void serial_intr_recv_enable(void){
    UARTICR = UARTINT_RX_ALL;
    UARTIMSC |= UARTINT_RX_ALL;
}

void serial_intr_recv_disable(void){
    UARTIMSC &= ~UARTINT_RX_ALL;
    UARTICR = UARTINT_RX_ALL;
}

void serial_intr_send_enable(void){
    UARTICR = UARTINT_TX;
    UARTIMSC |= UARTINT_TX;
}

void serial_intr_send_disable(void){
    UARTIMSC &= ~UARTINT_TX;
    UARTICR = UARTINT_TX;
}

int serial_intr_is_recv(void){
    return (UARTMIS & UARTINT_RX_ALL) != 0;
}

int serial_intr_is_send(void){
    return (UARTMIS & UARTINT_TX) != 0;
}

int serial_intr_is_send_enable(void){
    return (UARTIMSC & UARTINT_TX) != 0;
}

void serial_intr_clear_recv(void){
    UARTICR = UARTINT_RX_ALL;
}

void serial_intr_clear_send(void){
    UARTICR = UARTINT_TX;
}

/*
 * Polling output is retained for early boot and fatal kernel diagnostics.
 * Normal threads must send console output through consdrv.
 */
void serial_putc(char c){
    if (c == '\n') {
        serial_putc('\r');
    }

    while (!serial_is_send_enable()) {
        /* wait */
    }

    serial_send_byte((uint8_t)c);
}

void serial_puts(const char *s){
    while (*s) {
        serial_putc(*s++);
    }
}

void serial_put_uint(uint32_t value){
    char buffer[10];
    int index = 0;

    if (value == 0) {
        serial_putc('0');
        return;
    }

    while (value > 0) {
        buffer[index++] = (char)('0' + (value % 10u));
        value /= 10u;
    }

    while (index > 0) {
        serial_putc(buffer[--index]);
    }
}

void serial_put_int(int value){
    uint32_t magnitude;

    if (value < 0) {
        serial_putc('-');
        magnitude = 0u - (uint32_t)value;
    } else {
        magnitude = (uint32_t)value;
    }

    serial_put_uint(magnitude);
}

void serial_put_hex(uint32_t value){
    static const char hex_table[] = "0123456789ABCDEF";
    int shift;

    serial_puts("0x");

    for (shift = 28; shift >= 0; shift -= 4) {
        serial_putc(hex_table[(value >> shift) & 0x0fu]);
    }
}