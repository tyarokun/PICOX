#include "clock.h"
#include "serial.h"
#include "handler.h"
#include "interrupt.h"
#include "kernel.h"


static void runtime_init(void){
  uint32_t *src = &_data_load;
  uint32_t *dst = &_data_start;
  while (dst < &_data_end) *dst++ = *src++;
  for (dst = &_bss_start; dst < &_bss_end; dst++) *dst = 0;
}

void Reset_Handler(void){
  runtime_init();
  clock_init_125mhz();
  serial_init();
  softvec_init();
  picox_start(start_thread, "main", 0x800, 0, NULL);
  while(1);
}

void NMI_Handler(void){
  while(1);
}

void HardFault_Handler(void){
  INTR_DISABLE();
  serial_puts("HardFault\n");
  while(1);
}

/*
 * svc #0: 通常のシステムコール。PSP上にr4-r11、EXC_RETURN、CONTROLを保存する。
 * svc #1: OS起動時だけ使用。初期スレッドに切り替える。
 */
__attribute__((naked)) void SVC_Handler(void){
  __asm__ volatile (
    ".syntax unified             \n"  // アセンブリ構文設定をARMとThumbで共通化されたUnified Assembly Syntaxに設定
    "mov  r3, lr                 \n"    // r3←lr:EXC_RETURNが入っている (MSP or PSP)
    "movs r2, #4                 \n"    // 0b100(次の比較に使用)
    "tst  r3, r2                 \n"    // 2bit目(0b100)と比較(AND演算しフラグ更新)→0ならMSP,1ならPSP (例外発生前に使っていたスタックがわかる)
    "beq  1f                     \n"    // 2bit目が0なら"ラベル1"へジャンプ
    "mrs  r0, psp                \n"    // pspの場合 mrsはコピーする命令(r0←psp0)
    "b    2f                     \n"    
    "1:                          \n"    
    "mrs  r0, msp                \n"    // mspの場合 mrsはコピーする命令(r0←msp0)
    "2:                          \n"    // この時点でr0は例外フレームの先頭を指している {例外フレームの配置→オフセット:レジスタ=(0:r0),(4:r1),(8:r2),(12:r3),(16:r12),(20:lr),(24:pc),(28:xPSR)}
    "ldr  r1, [r0, #24]          \n"    // オフセット24(pc)のアドレスから内容を読み出す(次に実行する命令のアドレスが入っている)
    "movs r2, #2                 \n"
    "subs r1, r1, r2             \n"    // r1 = PC - 2 → svc #番号 (これでr1は、実行されたSVC命令のアドレスを指す)
    "ldrb r1, [r1]               \n"    // ldrb→SVC命令の下位1バイト(8bit)を読み出す(svc番号) r1 = svc番号
    "cmp  r1, #1                 \n"    // if(svc番号 == 1) Zフラグが1
    "beq  5f                     \n"    // 5へジャンプ

    /* 通常システムコールは常にThread mode + PSPから発行 */
    "mrs  r0, psp                \n"    
    "movs r2, #40                \n"    // 追加保存するコンテキストのサイズ分 (r8~r11:16byte, r4~r7:16byte, EXC_RETURN:4byte, CONTROL:4byte)
    "subs r0, r0, r2             \n"    // pspから40byte分引いて領域を確保
    "mov  r1, r0                 \n"    // r1←r0
    "movs r2, #16                \n"    // r2←16
    "adds r1, r1, r2             \n"    // r1(psp-40の位置から)16バイト進める
    "stmia r1!, {r4-r7}          \n"    // r4～r7をr1が指す位置から順番に保存
    "mov  r4, r8                 \n"
    "mov  r5, r9                 \n"
    "mov  r6, r10                \n"
    "mov  r7, r11                \n"
    "mov  r1, r0                 \n"    // r1←r0(psp-40)
    "stmia r1!, {r4-r7}          \n"    // r4～r7(r8～r11)をr1が指す位置から順番に保存
    "str  r3, [r0, #32]          \n"    // 最初に保存したEXC_RETURNをr0+32byteへ保存
    "mrs  r2, control            \n"    // r2←CONTROL(特権・非特権状態,Thread modeでMSPとPSPのどちらを使うか)
    "str  r2, [r0, #36]          \n"    // CONTROLをr0+36byteへ保存
    "msr  psp, r0                \n"    // psp←r0 コンテキスト保存領域の先頭を新しいpspとして設定
    "mov  r1, r0                 \n"    // r1←r0
    "movs r0, #1                 \n"    // r0←SOFTVEC_TYPE_SYSCALL
    "b    interrupt              \n"    // interrupt(r0, r1)

    "5:                          \n"
    "ldr  r0, [r0]               \n"    // r0←current->context
    "b    dispatch               \n"    // dispatch(r0)
    ".syntax divided             \n"  //アセンブリ構文設定を従来のDivided Syntaxへ戻す
  );
}

void PendSV_Handler(void){

}

void SysTick_Handler(void){

}
