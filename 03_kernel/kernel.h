#ifndef KERNEL_H
#define KERNEL_H

#include "define.h"
#include "syscall.h"

picox_thread_id_t picox_run(picox_func_t func, char *name, int stacksize, int argc, char *argv[]);
void picox_exit(void);
void picox_start(picox_func_t func, char *name, int stacksize, int argc, char *argv[]);
void picox_sysdown(void);
void picox_syscall(picox_syscall_type_t type, picox_syscall_param_t *param);

#endif
