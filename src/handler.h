#ifndef HANDLER_H
#define HANDLER_H

#include "define.h"

typedef enum{
    SOFTVEC_TYPE_SOFTERR = 0,
    SOFTVEC_TYPE_SYSCALL,
    SOFTVEC_TYPE_SERINTR,
    SOFTVEC_TYPE_TIMER,
    SOFTVEC_TYPE_NUM,
};

// 例外ハンドラ
void Reset_Handler(void);
void NMI_Handler(void);
void HardFault_Handler(void);
void SVC_Handler(void);
void PendSV_Handler(void);
void SysTick_Handler(void);
void UART0_IRQ_Handler(void);

int start_thread(int argc, char *argv[]);

// Systic control
void systick_init(void);
void systick_stop(void);

#endif
