#include "define.h"
#include "handler.h"
#include "interrupt.h"
#include "kernel.h"
#include "lib.h"
#include "serial.h"
#include "shell.h"

#define SHELL_LINE_SIZE 80

static volatile int serial_event_pending;

static void shell_serial_interrupt(void){
    int received = serial_rx_interrupt_handler();
    if (received > 0 && !serial_event_pending) {
        serial_event_pending = 1;
        picoxs_send(MSGBOX_ID_CONSINPUT, 0, NULL);
    }
}

static void shell_prompt(void){
    serial_puts("picox> ");
}

static void shell_help(void){
    serial_puts("commands:\n");
    serial_puts("  help       show this help\n");
    serial_puts("  echo TEXT  print TEXT\n");
    serial_puts("  version    show kernel version\n");
    serial_puts("  irqstat    show UART RX overflow count\n");
    serial_puts("  clear      clear terminal\n");
}

static char *skip_spaces(char *p){
    while (*p == ' ') {
        p++;
    }
    return p;
}

static void shell_execute(char *line){
    char *command;
    char *argument;
    command = skip_spaces(line);
    if (*command == '\0') {
        return;
    }
    argument = command;
    while (*argument != '\0' && *argument != ' ') {
        argument++;
    }
    if (*argument != '\0') {
        *argument++ = '\0';
        argument = skip_spaces(argument);
    }
    if (strcmp(command, "help") == 0) {
        shell_help();
    } else if (strcmp(command, "echo") == 0) {
        serial_puts(argument);
        serial_puts("\n");
    } else if (strcmp(command, "version") == 0) {
        serial_puts("PicoX 0.1 (RP2040)\n");
    } else if (strcmp(command, "irqstat") == 0) {
        serial_puts("UART RX overflow: ");
        serial_put_uint(serial_rx_overflow_count());
        serial_puts("\n");
    } else if (strcmp(command, "clear") == 0) {
        serial_puts("\033[2J\033[H");
    } else {
        serial_puts("unknown command: ");
        serial_puts(command);
        serial_puts("\n");
    }
}

static void shell_process_character(int c){
    static char line[SHELL_LINE_SIZE];
    static int length;
    static int previous_was_cr;
    if (c == '\n' && previous_was_cr) {
        previous_was_cr = 0;
        return;
    }
    if (c == '\r' || c == '\n') {
        previous_was_cr = (c == '\r');
        serial_puts("\n");
        line[length] = '\0';
        shell_execute(line);
        length = 0;
        shell_prompt();
        return;
    }
    previous_was_cr = 0;
    if (c == 0x03) { /* Ctrl-C */
        serial_puts("^C\n");
        length = 0;
        shell_prompt();
        return;
    }
    if (c == 0x0c) { /* Ctrl-L */
        serial_puts("\033[2J\033[H");
        shell_prompt();
        if (length > 0) {
            int i;
            for (i = 0; i < length; i++) {
                serial_putc(line[i]);
            }
        }
        return;
    }
    if (c == '\b' || c == 0x7f) {
        if (length > 0) {
            length--;
            serial_puts("\b \b");
        }
        return;
    }
    if (c >= ' ' && c <= '~') {
        if (length < SHELL_LINE_SIZE - 1) {
            line[length++] = (char)c;
            serial_putc((char)c);
        } else {
            serial_putc('\a');
        }
    }
}

int shell_system_task(int argc, char *argv[]){
    int c;
    serial_event_pending = 0;
    if (picox_setintr(SOFTVEC_TYPE_SERINTR, shell_serial_interrupt) != 0) {
        picox_sysdown();
    }
    /* softvec登録後にUART/NVICを有効化する。 */
    serial_enable_rx_interrupt();
    serial_puts("\nPicoX shell started\n");
    serial_puts("type 'help' for commands\n");
    shell_prompt();
    while (1) {
        picox_recv(MSGBOX_ID_CONSINPUT, NULL, NULL);
        serial_event_pending = 0;
        while ((c = serial_readc_nonblock()) >= 0) {
            shell_process_character(c);
        }
    }
    return 0;
}
