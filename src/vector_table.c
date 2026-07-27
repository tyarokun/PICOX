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
    SysTick_Handler,           // 15: SysTick   タイマ

    0,                          /* IRQ  0: TIMER_IRQ_0 */
    0,                          /* IRQ  1: TIMER_IRQ_1 */
    0,                          /* IRQ  2: TIMER_IRQ_2 */
    0,                          /* IRQ  3: TIMER_IRQ_3 */
    0,                          /* IRQ  4: PWM_IRQ_WRAP */
    0,                          /* IRQ  5: USBCTRL_IRQ */
    0,                          /* IRQ  6: XIP_IRQ */
    0,                          /* IRQ  7: PIO0_IRQ_0 */
    0,                          /* IRQ  8: PIO0_IRQ_1 */
    0,                          /* IRQ  9: PIO1_IRQ_0 */
    0,                          /* IRQ 10: PIO1_IRQ_1 */
    0,                          /* IRQ 11: DMA_IRQ_0 */
    0,                          /* IRQ 12: DMA_IRQ_1 */
    0,                          /* IRQ 13: IO_IRQ_BANK0 */
    0,                          /* IRQ 14: IO_IRQ_QSPI */
    0,                          /* IRQ 15: SIO_IRQ_PROC0 */
    0,                          /* IRQ 16: SIO_IRQ_PROC1 */
    0,                          /* IRQ 17: CLOCKS_IRQ */
    0,                          /* IRQ 18: SPI0_IRQ */
    0,                          /* IRQ 19: SPI1_IRQ */
    UART0_IRQ_Handler,          /* IRQ 20: UART0_IRQ */
    0,                          /* IRQ 21: UART1_IRQ */
    0,                          /* IRQ 22: ADC_IRQ_FIFO */
    0,                          /* IRQ 23: I2C0_IRQ */
    0,                          /* IRQ 24: I2C1_IRQ */
    0,                          /* IRQ 25: RTC_IRQ */
    0,                          /* IRQ 26: Reserved */
    0,                          /* IRQ 27: Reserved */
    0,                          /* IRQ 28: Reserved */
    0,                          /* IRQ 29: Reserved */
    0,                          /* IRQ 30: Reserved */
    0                           /* IRQ 31: Reserved */
};
