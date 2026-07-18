#include "serial.h"

int main(void)
{
    serial_init();

    serial_getc(); //Enterで開始(picoprobeのファームウェアではバッファサイズは32byte)
    serial_puts("\n");
    serial_puts("PICOX UART ready!\n");
    serial_puts("Type something. Pico will echo it back.\n");

    while (1) {
        char c = serial_getc();

        if (c == '\r' || c == '\n') {
            serial_puts("\n");
        } else if (c == 3) {
            serial_puts("\n^C received\n");
        } else {
            serial_putc(c);
        }
    }

    return 0;
}