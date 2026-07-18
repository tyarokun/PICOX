#ifndef DEFINE_H
#define DEFINE_H

#define NULL ((void *)0)

typedef unsigned char  uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int   uint32_t;

typedef uint32_t picox_thread_id_t;
typedef int  (*picox_func_t)(int argc, char *argv[]);
typedef void (*picox_handler_t)(void);

typedef enum{
    MSGBOX_ID_MSGBOX1 = 0,
    MSGBOX_ID_MSGBOX2,
    MSGBOX_ID_CONSINPUT,
    MSGBOX_ID_CONSOUTPUT,
    MSGBOX_ID_NUM,
} picox_msgbox_id_t;

#endif
