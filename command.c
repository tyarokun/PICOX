#include "define.h"
#include "serial.h"
#include "lib.h"
#include "command.h"

typedef int (*command_func)(char *argument);

typedef struct _command_entry {
    const char *name;
    command_func function;
    const char *description;
} command_entry;

static char *skip_spaces(char *p){
    while (*p == ' ') p++;
    return p;
}

static int command_help(char *argument);

static int command_echo(char *argument){
    serial_puts(argument);
    serial_puts("\n");
    return 0;
}

static int command_version(char *argument){
    (void)argument;
    serial_puts("PicoX 0.2 (RP2040)\n");
    return 0;
}

static int command_irqstat(char *argument){
    (void)argument;
    serial_puts("UART RX overflow: ");
    serial_put_uint(serial_rx_overflow_count());
    serial_puts("\n");
    return 0;
}

static int command_clear(char *argument){
    (void)argument;
    serial_puts("\033[2J\033[H");
    return 0;
}


static const command_entry commands[] = {
    {"help",    command_help,    "show this help"},
    {"echo",    command_echo,    "print text"},
    {"version", command_version, "show kernel version"},
    {"irqstat", command_irqstat, "show UART RX overflow count"},
    {"clear",   command_clear,   "clear terminal"},
};

#define COMMAND_COUNT ((int)(sizeof(commands) / sizeof(commands[0])))

static int command_help(char *argument){
    int i;
    (void)argument;
    serial_puts("commands:\n");
    for (i = 0; i < COMMAND_COUNT; i++) {
        serial_puts("  ");
        serial_puts(commands[i].name);
        serial_puts(" - ");
        serial_puts(commands[i].description);
        serial_puts("\n");
    }
    return 0;
}

int command_execute(char *line){
    char *command;
    char *argument;
    int i;
    command = skip_spaces(line);
    if (*command == '\0') return 0;
    argument = command;
    while (*argument && *argument != ' ') argument++;
    if (*argument) {
        *argument++ = '\0';
        argument = skip_spaces(argument);
    }
    for (i = 0; i < COMMAND_COUNT; i++) {
        if (strcmp(command, commands[i].name) == 0) {
            return commands[i].function(argument);
        }
    }
    serial_puts("unknown command: ");
    serial_puts(command);
    serial_puts("\n");
    return -1;
}