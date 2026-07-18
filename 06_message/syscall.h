#ifndef SYSCALL_H
#define SYSCALL_H

#include "define.h"

typedef enum{
  SYSCALL_TYPE_RUN = 0,
  SYSCALL_TYPE_EXIT,
  SYSCALL_TYPE_WAIT,
  SYSCALL_TYPE_SLEEP,
  SYSCALL_TYPE_WAKEUP,
  SYSCALL_TYPE_GETID,
  SYSCALL_TYPE_CHPRI,
  SYSCALL_TYPE_MALLOC,
  SYSCALL_TYPE_FREE,
  SYSCALL_TYPE_SEND,
  SYSCALL_TYPE_RECV,
} picox_syscall_type_t;

typedef struct{
  union{
    struct{
      picox_func_t func;
      char *name;
      int priority;
      int stacksize;
      int argc;
      char **argv;
      picox_thread_id_t ret;
    }run;
    struct{
      int dummy;
    }exit;
    struct{
      int ret;
    }wait;
    struct{
      int ret;
    }sleep;
    struct{
      picox_thread_id_t id;
      int ret;
    }wakeup;
    struct{
      picox_thread_id_t ret;
    }getid;
    struct{
      int priority;
      int ret;
    }chpri;
    struct{
      int size;
      void *ret;
    }malloc;
    struct{
      char *p;
      int ret;
    }free;
    struct{
      picox_msgbox_id_t id;
      int size;
      char *p;
      int ret;
    }send;
    struct{
      picox_msgbox_id_t id;
      int *sizep;
      char **pp;
      int ret;
    }recv;
  }un;
} picox_syscall_param_t;

#endif
