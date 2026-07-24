#include "define.h"
#include "kernel.h"
#include "syscall.h"
#include "interrupt.h"


// システムコール
picox_thread_id_t picox_run(picox_func_t func, char *name, int priority, int stacksize, int argc, char *argv[]){
  picox_syscall_param_t param;
  param.un.run.func = func;
  param.un.run.name = name;
  param.un.run.priority = priority;
  param.un.run.stacksize = stacksize;
  param.un.run.argc = argc;
  param.un.run.argv = argv;
  picox_syscall(SYSCALL_TYPE_RUN, &param);
  return param.un.run.ret;
}

void picox_exit(void){
  picox_syscall(SYSCALL_TYPE_EXIT, NULL);
  while(1);
}

int picox_wait(void){
  picox_syscall_param_t param;
  picox_syscall(SYSCALL_TYPE_WAIT, &param);
  return param.un.wait.ret;
}

int picox_sleep(void){
  picox_syscall_param_t param;
  picox_syscall(SYSCALL_TYPE_SLEEP, &param);
  return param.un.sleep.ret;
}

int picox_wakeup(picox_thread_id_t id){
  picox_syscall_param_t param;
  param.un.wakeup.id = id;
  picox_syscall(SYSCALL_TYPE_WAKEUP, &param);
  return param.un.wakeup.ret;
}

picox_thread_id_t picox_getid(void){
  picox_syscall_param_t param;
  picox_syscall(SYSCALL_TYPE_GETID, &param);
  return param.un.getid.ret;
}

int picox_chpri(int priority){
  picox_syscall_param_t param;
  param.un.chpri.priority = priority;
  picox_syscall(SYSCALL_TYPE_CHPRI, &param);
  return param.un.chpri.ret;
}

void *picox_malloc(int size){
  picox_syscall_param_t param;
  param.un.malloc.size = size;
  picox_syscall(SYSCALL_TYPE_MALLOC, &param);
  return param.un.malloc.ret;
}

int picox_free(void *p){
  picox_syscall_param_t param;
  param.un.free.p = p;
  picox_syscall(SYSCALL_TYPE_FREE, &param);
  return param.un.free.ret;
}

int picox_send(picox_msgbox_id_t id, int size, char *p){
  picox_syscall_param_t param;
  param.un.send.id = id;
  param.un.send.size = size;
  param.un.send.p = p;
  picox_syscall(SYSCALL_TYPE_SEND, &param);
  return param.un.send.ret;
}

picox_thread_id_t picox_recv(picox_msgbox_id_t id, int *sizep, char **pp){
  picox_syscall_param_t param;
  param.un.recv.id = id;
  param.un.recv.sizep = sizep;
  param.un.recv.pp = pp;
  picox_syscall(SYSCALL_TYPE_RECV, &param);
  return param.un.recv.ret;
}

int picox_setintr(softvec_type_t type, picox_handler_t handler){
  picox_syscall_param_t param;
  param.un.setintr.type = type;
  param.un.setintr.handler = handler;
  picox_syscall(SYSCALL_TYPE_SETINTR, &param);
  return param.un.setintr.ret;
}

// サービスコール
int picoxs_wakeup(picox_thread_id_t id){
  picox_syscall_param_t param;
  param.un.wakeup.id = id;
  picox_srvcall(SYSCALL_TYPE_WAKEUP, &param);
  return param.un.wakeup.ret;
}

void *picoxs_malloc(int size){
  picox_syscall_param_t param;
  param.un.malloc.size = size;
  picox_srvcall(SYSCALL_TYPE_MALLOC, &param);
  return param.un.malloc.ret;
}

int picoxs_free(void *p){
  picox_syscall_param_t param;
  param.un.free.p = p;
  picox_srvcall(SYSCALL_TYPE_FREE, &param);
  return param.un.free.ret;
}

int picoxs_send(picox_msgbox_id_t id, int size, char *p){
  picox_syscall_param_t param;
  param.un.send.id = id;
  param.un.send.size = size;
  param.un.send.p = p;
  picox_srvcall(SYSCALL_TYPE_SEND, &param);
  return param.un.send.ret;
}