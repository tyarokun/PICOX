#include "define.h"
#include "kernel.h"
#include "syscall.h"

picox_thread_id_t picox_run(picox_func_t func, char *name, int stacksize, int argc, char *argv[]){
  picox_syscall_param_t param;
  param.un.run.func = func;
  param.un.run.name = name;
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
