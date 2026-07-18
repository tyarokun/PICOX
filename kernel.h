#ifndef KERNEL_H
#define KERNEL_H

#include "define.h"
#include "syscall.h"
#include "interrupt.h"

// システムコール
picox_thread_id_t picox_run(picox_func_t func, char *name, int priority, int stacksize, int argc, char *argv[]);
void picox_exit(void);
int picox_wait(void);
int picox_sleep(void);
int picox_wakeup(picox_thread_id_t id);
picox_thread_id_t picox_getid(void);
int picox_chpri(int priority);
void *picox_malloc(int size);
int picox_free(void *p);
int picox_send(picox_msgbox_id_t id, int size, char *p);
picox_msgbox_id_t picox_recv(picox_msgbox_id_t id, int *sizep, char **pp);
int picox_setintr(softvec_type_t type, picox_handler_t handler);

// サービスコール
int picoxs_wakeup(picox_thread_id_t id);
void *picoxs_kmalloc(int size);
int picoxs_free(void *p);
int picoxs_send(picox_msgbox_id_t id, int size, char *p);

void picox_start(picox_func_t func, char *name, int priority, int stacksize, int argc, char *argv[]);
void picox_sysdown(void);
void picox_syscall(picox_syscall_type_t type, picox_syscall_param_t *param);
void picox_srvcall(picox_syscall_type_t type, picox_syscall_param_t *param);

#endif
