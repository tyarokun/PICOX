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
    MSGBOX_ID_CONSRX,
    MSGBOX_ID_CONSTX,
    MSGBOX_ID_APPREQUEST,
    MSGBOX_ID_CMDEND,
    MSGBOX_ID_APPEND,
    MSGBOX_ID_SDREQUEST,
    MSGBOX_ID_SDRESULT,
    MSGBOX_ID_NUM,
} picox_msgbox_id_t;

#define THREAD_NUM       10
#define THREAD_NAME_SIZE 15
#define PRIORITY_NUM 16

typedef struct {
    picox_thread_id_t id;
    char name[THREAD_NAME_SIZE];
    int priority;
} picox_thread_info_t;

#endif
