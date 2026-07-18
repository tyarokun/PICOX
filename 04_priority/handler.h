#ifndef HANDLER_H
#define HANDLER_H

#include "define.h"

#define SOFTVEC_TYPE_NUM      3
#define SOFTVEC_TYPE_SOFTERR  0
#define SOFTVEC_TYPE_SYSCALL  1
#define SOFTVEC_TYPE_SERINTR  2

// 例外ハンドラ
void Reset_Handler(void);
void NMI_Handler(void);
void HardFault_Handler(void);
void SVC_Handler(void);
void PendSV_Handler(void);
void SysTick_Handler(void);


int start_thread(int argc, char *argv[]);

#endif
