#ifndef EXEC_H
#define EXEC_H

typedef struct{
    char filename[13];
    int background;
} exec_request_t;

int exec_main(int argc, char *argv[]);

#endif