#include "define.h"
#include "app_memory.h"

// MPU_CTRLレジスタ
#define MPU_CTRL (*(volatile uint32_t *)0xE000ED94)
#define MPU_CTRL_ENABLE (1 << 0)
#define MPU_CTRL_PRIVDEFENA (1 << 2)

// MPU_RNRレジスタ
#define MPU_RNR (*(volatile uint32_t *)0xE000ED98)

// MPU_RBARレジスタ
#define MPU_RBAR (*(volatile uint32_t *)0xE000ED9c)


// MPU_RASRレジスタ
#define MPU_RASR (*(volatile uint32_t *)0xE000EDa0)
#define MPU_RASR_ENABLE (1 << 0)
#define MPU_RASR_SIZE (16 << 1) // Region size = 2^(SIZE + 1) → appの領域は128KB = 2^17 → SIZE = 16


void mpu_init(void){
    // MPUを無効化
    __asm__ volatile ("dsb");
    MPU_CTRL = 0;

    MPU_RNR = 0;

    MPU_RBAR = _app_load_start;
    
    // MPUを有効化
    MPU_CTRL = MPU_CTRL_PRIVDEFENA;
    __asm__ volatile ("dsb");
    __asm__ volatile ("isb");
    MPU_CTRL |= MPU_CTRL_ENABLE;
    __asm__ volatile ("dsb");
    __asm__ volatile ("isb");
}