#include "define.h"
#include "handler.h"
#include "interrupt.h"
#include "kernel.h"
#include "serial.h"
#include "lib.h"
#include "shell.h"

#define SHELL_RECV_BUFFER_SIZE 48

#define SHELL_SEND_BUFFER_SIZE 512
#define SHELL_SEND_BUFFER_MASK (SHELL_SEND_BUFFER_SIZE - 1)

#if (SHELL_SEND_BUFFER_SIZE & SHELL_SEND_BUFFER_MASK) != 0
#error SHELL_SEND_BUFFER_SIZE must be a power of two
#endif

typedef int (*command_func_t)(char *argument);

typedef struct {
    const char *name;
    command_func_t function;
    const char *description;
} command_entry;

typedef struct {
    char recv_buffer[SHELL_RECV_BUFFER_SIZE];
    int recv_length;
    int previous_was_cr;

    uint8_t send_buffer[SHELL_SEND_BUFFER_SIZE];
    volatile uint32_t send_head;
    volatile uint32_t send_tail;
} shell_console;

static shell_console console;

static uint32_t send_buffer_used(const shell_console *cons){
    return (cons->send_tail - cons->send_head) &
           SHELL_SEND_BUFFER_MASK;
}

static uint32_t send_buffer_free(const shell_console *cons){
    return (SHELL_SEND_BUFFER_SIZE - 1u) -
           send_buffer_used(cons);
}

static int send_push_raw(shell_console *cons, uint8_t value){
    uint32_t next =
        (cons->send_tail + 1u) & SHELL_SEND_BUFFER_MASK;

    if (next == cons->send_head) {
        return -1;
    }

    cons->send_buffer[cons->send_tail] = value;
    cons->send_tail = next;

    return 0;
}

static int send_required_size(const char *data, int length){
    int i;
    int required = 0;

    for (i = 0; i < length; i++) {
        required += (data[i] == '\n') ? 2 : 1;
    }

    return required;
}

static int send_push_text(shell_console *cons, const char *data, int length){
    int i;
    int required;

    required = send_required_size(data, length);

    if ((uint32_t)required > send_buffer_free(cons)) {
        return -1;
    }

    for (i = 0; i < length; i++) {
        if (data[i] == '\n') {
            send_push_raw(cons, (uint8_t)'\r');
        }

        send_push_raw(cons, (uint8_t)data[i]);
    }

    return 0;
}

static void send_drain(shell_console *cons){
    while (cons->send_head != cons->send_tail &&
           serial_is_send_enable()) {
        serial_send_byte(cons->send_buffer[cons->send_head]);
        cons->send_head =
            (cons->send_head + 1u) & SHELL_SEND_BUFFER_MASK;
    }

    if (cons->send_head == cons->send_tail) {
        serial_intr_send_disable();
    } else if (!serial_intr_is_send_enable()) {
        serial_intr_send_enable();
    }
}

static int send_text( shell_console *cons, const char *data, int length){
    int result;

    INTR_DISABLE();

    result = send_push_text(cons, data, length);

    if (result == 0) {
        send_drain(cons);
    }

    INTR_ENABLE();

    return result;
}

static void send_text_from_interrupt(shell_console *cons, const char *data, int length){
    if (send_push_text(cons, data, length) == 0) {
        send_drain(cons);
    }
}

static int send_write_length(const char *data, int length){
    return send_text(&console, data, length);
}

static int send_write(const char *text){
    return send_write_length(text, strlen(text));
}

static void send_input_line(shell_console *cons){
    char *message;
    int size = cons->recv_length + 1;

    message = (char *)picoxs_kmalloc(size);
    memcpy(message, cons->recv_buffer, cons->recv_length);
    message[cons->recv_length] = '\0';

    picoxs_send(MSGBOX_ID_CONSINPUT, size, message);

    cons->recv_length = 0;
}

static void process_received_character(shell_console *cons, int c){
    if (c == '\n' && cons->previous_was_cr) {
        cons->previous_was_cr = 0;
        return;
    }

    if (c == '\r') {
        cons->previous_was_cr = 1;
        c = '\n';
    } else {
        cons->previous_was_cr = 0;
    }

    if (c == '\n') {
        send_text_from_interrupt(cons, "\n", 1);
        send_input_line(cons);
        return;
    }

    if (c == 0x03) {
        cons->recv_length = 0;
        send_text_from_interrupt(cons, "^C\n", 3);
        send_input_line(cons);
        return;
    }

    if (c == '\b' || c == 0x7f) {
        if (cons->recv_length > 0) {
            cons->recv_length--;
            send_text_from_interrupt(cons, "\b \b", 3);
        }

        return;
    }

    if (c >= ' ' && c <= '~') {
        if (cons->recv_length < SHELL_RECV_BUFFER_SIZE - 1) {
            char character = (char)c;

            cons->recv_buffer[cons->recv_length++] = character;
            send_text_from_interrupt(cons, &character, 1);
        } else {
            char bell = '\a';
            send_text_from_interrupt(cons, &bell, 1);
        }
    }
}

static void shell_interrupt(void){
    if (serial_intr_is_recv() || serial_is_recv_enable()) {
        while (serial_is_recv_enable()) {
            process_received_character(
                &console,
                (int)serial_recv_byte());
        }

        serial_intr_clear_recv();
    }

    if (serial_intr_is_send()) {
        serial_intr_clear_send();
    }

    if (console.send_head != console.send_tail) {
        send_drain(&console);
    } else {
        serial_intr_send_disable();
    }
}

static char *skip_spaces(char *p){
    while (*p == ' ') {
        p++;
    }

    return p;
}

static int command_help(char *argument);
static int command_echo(char *argument);
static int command_version(char *argument);
static int command_clear(char *argument);

static const command_entry commands[] = {
    {"help",    command_help,    "show this help"},
    {"echo",    command_echo,    "print text"},
    {"version", command_version, "show kernel version"},
    {"clear",   command_clear,   "clear terminal"},
};

#define COMMAND_COUNT \
    ((int)(sizeof(commands) / sizeof(commands[0])))

static int command_help(char *argument){
    int i;

    (void)argument;

    send_write("commands:\n");

    for (i = 0; i < COMMAND_COUNT; i++) {
        send_write("  ");
        send_write(commands[i].name);
        send_write(" - ");
        send_write(commands[i].description);
        send_write("\n");
    }

    return 0;
}

static int command_echo(char *argument){
    send_write(argument);
    send_write("\n");

    return 0;
}

static int command_version(char *argument){
    (void)argument;

    send_write("PicoX 0.2 (RP2040)\n");

    return 0;
}

static int command_clear(char *argument){
    (void)argument;

    send_write("\033[2J\033[H");

    return 0;
}

static int command_execute(char *line){
    char *command;
    char *argument;
    int i;

    command = skip_spaces(line);

    if (*command == '\0') {
        return 0;
    }

    argument = command;

    while (*argument && *argument != ' ') {
        argument++;
    }

    if (*argument) {
        *argument++ = '\0';
        argument = skip_spaces(argument);
    }

    for (i = 0; i < COMMAND_COUNT; i++) {
        if (strcmp(command, commands[i].name) == 0) {
            return commands[i].function(argument);
        }
    }

    send_write("unknown command: ");
    send_write(command);
    send_write("\n");

    return -1;
}

int shell_main(int argc, char *argv[]){
    char *line;
    int size;

    memset(&console, 0, sizeof(console));

    serial_intr_recv_disable();
    serial_intr_send_disable();

    if (picox_setintr(SOFTVEC_TYPE_SERINTR, shell_interrupt) != 0) {
        picox_sysdown();
    }

    serial_interrupt_enable();
    serial_intr_recv_enable();

    send_write("\nPicoX shell started\n");

    for (;;) {
        send_write("picox> ");
        picox_recv(MSGBOX_ID_CONSINPUT, &size, &line);
        if (!line || size <= 0) {
            if (line) {
                picox_free(line);
            }
            continue;
        }
        line[size - 1] = '\0';
        command_execute(line);
        picox_free(line);
    }

    return 0;
}