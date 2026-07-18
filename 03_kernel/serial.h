#ifndef SERIAL_H
#define SERIAL_H

void serial_init(void);
void serial_putc(char c);
void serial_puts(const char *s);
char serial_getc(void);
int serial_getc_nonblock(void);
void serial_gets(char *buf, int size);

#endif
