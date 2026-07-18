#ifndef INTERRUPT_H
#define INTERRUPT_H

#include "define.h"

typedef short softvec_type_t;
typedef void (*softvec_handler_t)(softvec_type_t type, uint32_t *sp);

// 割り込みマスク操作
#define INTR_ENABLE()  __asm volatile ("cpsie i")   //割り込み有効化
#define INTR_DISABLE() __asm volatile ("cpsid i")   //割り込み無効化 (割り込み禁止)

// 宣言
int softvec_init(void);
int softvec_setintr(softvec_type_t type, softvec_handler_t handler);
void interrupt(softvec_type_t type, uint32_t *sp);

#endif
