#ifndef SERIAL_H
#define SERIAL_H

#include "define.h"

/*
 * RP2040 UART0 low-level driver.
 *
 * This layer only operates the UART hardware.  Line editing, buffering,
 * and thread-to-thread communication belong to consdrv.c.
 */
void serial_init(void);

/* Polling API used during boot and fatal-error reporting. */
void serial_putc(char c);
void serial_puts(const char *s);
void serial_put_uint(uint32_t value);
void serial_put_int(int value);
void serial_put_hex(uint32_t value);

/* Byte-oriented UART API used by the console driver. */
int serial_is_send_enable(void);
void serial_send_byte(uint8_t value);
int serial_is_recv_enable(void);
uint8_t serial_recv_byte(void);

/* UART0/NVIC interrupt control. */
void serial_interrupt_enable(void);
void serial_interrupt_disable(void);

void serial_intr_recv_enable(void);
void serial_intr_recv_disable(void);
void serial_intr_send_enable(void);
void serial_intr_send_disable(void);

int serial_intr_is_recv(void);
int serial_intr_is_send(void);
int serial_intr_is_send_enable(void);

void serial_intr_clear_recv(void);
void serial_intr_clear_send(void);

#endif
