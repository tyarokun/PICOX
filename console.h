#ifndef CONSOLE_H
#define CONSOLE_H

int console_init(void);
void console_prompt(void);
int console_readline(char *buffer, int size);

#endif