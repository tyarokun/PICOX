#include "handler.h"

// ベクタテーブルの先頭に入れるスタックのアドレス
extern unsigned int _stack_top;


//ベクタテーブル
void (*vector_table[])(void) = {
    (void (*)(void))&_stack_top,  // 0: Initial Stack Pointer (MSP)
    Reset_Handler,             // 1: Reset Handler
    NMI_Handler,               // 2: NMI
    HardFault_Handler,         // 3: HardFault  不正メモリアクセスやスタックオーバーフロー
    0,                         // 4: Reserved
    0,                         // 5: Reserved
    0,                         // 6: Reserved
    0,                         // 7: Reserved
    0,                         // 8: Reserved
    0,                         // 9: Reserved
    0,                         // 10: Reserved
    SVC_Handler,               // 11: SVC   システムコール
    0,                         // 12: Reserved
    0,                         // 13: Reserved
    PendSV_Handler,            // 14: PendSV    コンテキストスイッチに使用
    SysTick_Handler            // 15: SysTick   タイマ
};
