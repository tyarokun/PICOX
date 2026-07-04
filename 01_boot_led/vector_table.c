#include "handler.h"

// ベクタテーブルの先頭に入れるスタックのアドレス
extern unsigned int _stack_top;

//ベクタテーブル
void (*vector_table[])(void) = {
    (void (*)(void))&_stack_top,  // 0: Initial Stack Pointer
    Reset_Handler,             // 1: Reset Handler
    Default_Handler,           // 2: NMI
    Default_Handler,           // 3: HardFault
    0,                         // 4: Reserved
    0,                         // 5: Reserved
    0,                         // 6: Reserved
    0,                         // 7: Reserved
    0,                         // 8: Reserved
    0,                         // 9: Reserved
    0,                         // 10: Reserved
    Default_Handler,           // 11: SVC
    0,                         // 12: Reserved
    0,                         // 13: Reserved
    Default_Handler,           // 14: PendSV
    Default_Handler            // 15: SysTick
};