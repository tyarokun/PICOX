#ifndef SERIAL_H
#define SERIAL_H

#include "define.h"

void serial_init(void);
void serial_putc(char c);
void serial_puts(const char *s);

void serial_put_uint(uint32_t value);
void serial_put_int(int value);
void serial_put_hex(uint32_t value);

char serial_getc(void);
int serial_getc_nonblock(void);
void serial_gets(char *buf, int size);

#endif