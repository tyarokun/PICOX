#include "define.h"
#include "kernel.h"
#include "serial.h"
#include "shell.h"
#include "console.h"
#include "command.h"

#define SHELL_LINE_SIZE 80

int shell_system_task(int argc, char *argv[]){
    char line[SHELL_LINE_SIZE];
    (void)argc;
    (void)argv;

    console_init();
    serial_puts("\nPicoX shell started\n");
    serial_puts("type 'help' for commands\n");

    while (1){
        console_prompt();
        if (console_readline(line, sizeof(line)) >= 0) {
            command_execute(line);
        }
    }
    return 0;
}