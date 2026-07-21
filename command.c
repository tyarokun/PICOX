#include "define.h"
#include "kernel.h"
#include "lib.h"
#include "consdrv.h"
#include "command.h"

typedef int (*command_func_t)(char *argument);

typedef struct {
    const char *name;
    command_func_t function;
    const char *description;
} command_entry;

static int send_use(int index)
{
    char *message = (char *)picox_malloc(2);

    message[0] = (char)index;
    message[1] = (char)CONSDRV_CMD_USE;

    return picox_send(MSGBOX_ID_CONSOUTPUT, 2, message);
}

static int send_write_length(const char *data, int length)
{
    char *message;
    int chunk;
    int sent = 0;

    while (length > 0) {
        chunk = length;

        if (chunk > CONSDRV_WRITE_CHUNK_SIZE) {
            chunk = CONSDRV_WRITE_CHUNK_SIZE;
        }

        message = (char *)picox_malloc(chunk + 2);

        message[0] = (char)CONSDRV_DEFAULT_DEVICE;
        message[1] = (char)CONSDRV_CMD_WRITE;
        memcpy(message + 2, data, chunk);

        picox_send(
            MSGBOX_ID_CONSOUTPUT,
            chunk + 2,
            message);

        data += chunk;
        length -= chunk;
        sent += chunk;
    }

    return sent;
}

static int send_write(const char *text)
{
    return send_write_length(text, strlen(text));
}

static char *skip_spaces(char *p)
{
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

static int command_help(char *argument)
{
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

static int command_echo(char *argument)
{
    send_write(argument);
    send_write("\n");

    return 0;
}

static int command_version(char *argument)
{
    (void)argument;

    send_write("PicoX 0.2 (RP2040)\n");

    return 0;
}

static int command_clear(char *argument)
{
    (void)argument;

    send_write("\033[2J\033[H");

    return 0;
}

static int command_execute(char *line)
{
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

int command_main(int argc, char *argv[])
{
    char *line;
    int size;

    (void)argc;
    (void)argv;

    send_use(CONSDRV_DEFAULT_DEVICE);

    send_write("\nPicoX shell started\n");
    send_write("type 'help' for commands\n");

    for (;;) {
        send_write("picox> ");

        picox_recv(
            MSGBOX_ID_CONSINPUT,
            &size,
            &line);

        if (!line || size <= 0) {
            if (line) {
                picox_free(line);
            }

            continue;
        }

        /*
         * consdrv includes the terminating NUL in the message.
         */
        line[size - 1] = '\0';

        command_execute(line);
        picox_free(line);
    }

    return 0;
}
