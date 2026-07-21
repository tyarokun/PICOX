#include "define.h"
#include "handler.h"
#include "kernel.h"
#include "serial.h"
#include "lib.h"
#include "console.h"

#define CONSOLE_LINE_SIZE 80

static volatile int serial_event_pending;
static char line[CONSOLE_LINE_SIZE];
static int length;
static int previous_was_cr;

static void console_serial_interrupt(void){
    int received = serial_rx_interrupt_handler();
    if (received > 0 && !serial_event_pending) {
        serial_event_pending = 1;
        picoxs_send(MSGBOX_ID_CONSINPUT, 0, NULL);
    }
}

void console_prompt(void){
    serial_puts("picox> ");
}

static int finish_line(char *output, int output_size){
    int copy_size;
    line[length] = '\0';
    copy_size = length;
    if (copy_size >= output_size) copy_size = output_size - 1;
    if (copy_size < 0) copy_size = 0;
    if (output_size > 0) {
        memcpy(output, line, copy_size);
        output[copy_size] = '\0';
    }
    length = 0;
    return copy_size;
}

static int process_character(int c, char *output, int output_size){
    if (c == '\n' && previous_was_cr) {
        previous_was_cr = 0;
        return -1;
    }
    if (c == '\r' || c == '\n') {
        previous_was_cr = (c == '\r');
        serial_puts("\n");
        return finish_line(output, output_size);
    }
    previous_was_cr = 0;
    if (c == 0x03) { /* Ctrl-C */
        serial_puts("^C\n");
        length = 0;
        if (output_size > 0) output[0] = '\0';
        return 0;
    }
    if (c == 0x0c) { /* Ctrl-L */
        int i;
        serial_puts("\033[2J\033[H");
        console_prompt();
        for (i = 0; i < length; i++) serial_putc(line[i]);
        return -1;
    }
    if (c == '\b' || c == 0x7f) {
        if (length > 0) {
            length--;
            serial_puts("\b \b");
        }
        return -1;
    }
    if (c >= ' ' && c <= '~') {
        if (length < CONSOLE_LINE_SIZE - 1) {
            line[length++] = (char)c;
            serial_putc((char)c);
        } else {
            serial_putc('\a');
        }
    }
    return -1;
}

int console_init(void){
    serial_event_pending = 0;
    length = 0;
    previous_was_cr = 0;
    memset(line, 0, sizeof(line));
    if (picox_setintr(SOFTVEC_TYPE_SERINTR, console_serial_interrupt) != 0) {
        return -1;
    }
    serial_enable_rx_interrupt();
    return 0;
}

int console_readline(char *buffer, int size){
    int c;
    int result;
    if (!buffer || size <= 0) return -1;
    while(1){
        if (serial_event_pending) {
            picox_recv(MSGBOX_ID_CONSINPUT, NULL, NULL);
            serial_event_pending = 0;
        }
        while ((c = serial_readc_nonblock()) >= 0) {
            result = process_character(c, buffer, size);
            if (result >= 0) return result;
        }
        picox_recv(MSGBOX_ID_CONSINPUT, NULL, NULL);
        serial_event_pending = 0;
    }
}