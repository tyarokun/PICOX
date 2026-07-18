#include "define.h"
#include "handler.h"
#include "interrupt.h"

static softvec_handler_t softvec[SOFTVEC_TYPE_NUM];

int softvec_init(void){
  int type;
  for (type = 0; type < SOFTVEC_TYPE_NUM; type++){
    softvec[type] = NULL;
  }
  return 0;
}

int softvec_setintr(softvec_type_t type, softvec_handler_t handler){
  if (type < 0 || type >= SOFTVEC_TYPE_NUM) return -1;
  softvec[type] = handler;
  return 0;
}

void interrupt(softvec_type_t type, uint32_t *sp){
  softvec_handler_t handler;
  if (type < 0 || type >= SOFTVEC_TYPE_NUM){
    return;
  }
  handler = softvec[type];
  if (handler){
    handler(type, sp);
  }
}