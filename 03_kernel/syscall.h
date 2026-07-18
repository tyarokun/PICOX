#ifndef SYSCALL_H
#define SYSCALL_H

#include "define.h"

typedef enum{
  SYSCALL_TYPE_RUN = 0,
  SYSCALL_TYPE_EXIT,
} picox_syscall_type_t;

typedef struct{
  union{
    struct{
      picox_func_t func;
      char *name;
      int stacksize;
      int argc;
      char **argv;
      picox_thread_id_t ret;
    }run;
    struct{
      int dummy;
    }exit;
  }un;
} picox_syscall_param_t;

#endif
