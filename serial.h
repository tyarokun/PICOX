#ifndef SERIAL_H
#define SERIAL_H

#include "define.h"

void serial_init(void);

/* 送信側はポーリング方式。 */
void serial_putc(char c);
void serial_puts(const char *s);
void serial_put_uint(uint32_t value);
void serial_put_int(int value);
void serial_put_hex(uint32_t value);

/* 従来のポーリング受信API。 */
char serial_getc(void);
int serial_getc_nonblock(void);
void serial_gets(char *buf, int size);

/* UART0受信割り込み用API。 */
void serial_enable_rx_interrupt(void);
void serial_disable_rx_interrupt(void);
int serial_rx_interrupt_handler(void);
int serial_readc_nonblock(void);
uint32_t serial_rx_overflow_count(void);

#endif