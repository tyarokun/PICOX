#include "define.h"
#include "serial.h"
#include "app_memory.h"

extern uint32_t _appstack, _appstack_end;

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
#define MPU_RASR_XN          (1u << 28)

// Privileged RW / Unprivileged RW
#define MPU_RASR_AP_FULL     (3u << 24)

// Privileged RO / Unprivileged RO
#define MPU_RASR_AP_RO       (6u << 24)


// Normal memory attributes
#define MPU_RASR_C           (1u << 17)
#define MPU_RASR_B           (1u << 16)
#define MPU_RASR_NORMAL      (MPU_RASR_C | MPU_RASR_B)

/* 4KB = 2^12 → SIZE = 11 */
#define MPU_RASR_SIZE_4KB    (11u << 1)

/*
 * Device memory属性
 *
 * TEX = 0
 * S   = 1
 * C   = 0
 * B   = 1
 */
#define MPU_RASR_S           (1u << 18)
#define MPU_RASR_DEVICE      (MPU_RASR_S | MPU_RASR_B)

typedef struct {
    uint32_t base;
    uint32_t size;
    uint32_t size_field;
} mpu_region_info_t;

static mpu_region_info_t mpu_region_from_range(uint32_t start, uint32_t end){
    mpu_region_info_t result;
    uint32_t size = 32u;
    while (1) {
        uint32_t base = start & ~(size - 1u);
        if (base + size >= end) {
            uint32_t log2_size = 0;
            uint32_t tmp = size;
            while (tmp > 1u) {
                tmp >>= 1;
                log2_size++;
            }
            result.base = base;
            result.size = size;
            result.size_field = (log2_size - 1u) << 1;
            return result;
        }
        size <<= 1;
    }
}

void mpu_set_region(uint32_t region, uint32_t base, uint32_t rasr){
    MPU_RNR = region;
    MPU_RBAR = base;
    MPU_RASR = rasr;
}

void mpu_init(void){
    mpu_region_info_t app;
    mpu_region_info_t appstack;

    app = mpu_region_from_range((uint32_t)&_app_load_start, (uint32_t)&_app_load_end);
    appstack = mpu_region_from_range((uint32_t)&_appstack, (uint32_t)&_appstack_end);

    // MPUを一旦無効化
    __asm__ volatile ("dsb");
    __asm__ volatile ("isb");
    MPU_CTRL = 0;

    // 範囲を設定
    mpu_set_region(0, app.base, MPU_RASR_AP_FULL | MPU_RASR_NORMAL | app.size_field | MPU_RASR_ENABLE);
    mpu_set_region(1, appstack.base, /*MPU_RASR_XN |*/ MPU_RASR_AP_FULL | MPU_RASR_NORMAL | appstack.size_field | MPU_RASR_ENABLE);
    mpu_set_region(2, 0x10000000u, MPU_RASR_AP_RO | MPU_RASR_NORMAL | (20u << 1) | MPU_RASR_ENABLE);
    mpu_set_region(3, 0x40014000u, MPU_RASR_XN | MPU_RASR_AP_FULL | MPU_RASR_DEVICE | MPU_RASR_SIZE_4KB | MPU_RASR_ENABLE); // IO_BANK0_BASE
    mpu_set_region(4, 0x40054000u, MPU_RASR_XN | MPU_RASR_AP_FULL | MPU_RASR_DEVICE | MPU_RASR_SIZE_4KB | MPU_RASR_ENABLE); // TIMER_BASE
    mpu_set_region(5, 0xD0000000u, MPU_RASR_XN | MPU_RASR_AP_FULL | MPU_RASR_DEVICE | MPU_RASR_SIZE_4KB | MPU_RASR_ENABLE); // SIO
    serial_puts("APP base = ");
    serial_put_hex(app.base);
    serial_puts("\n");

    serial_puts("APP size = ");
    serial_put_hex(app.size);
    serial_puts("\n");

    serial_puts("STACK base = ");
    serial_put_hex(appstack.base);
    serial_puts("\n");

    serial_puts("STACK size = ");
    serial_put_hex(appstack.size);
    serial_puts("\n");
    // MPUを有効化
    MPU_CTRL = MPU_CTRL_PRIVDEFENA;
    __asm__ volatile ("dsb");
    __asm__ volatile ("isb");
    MPU_CTRL |= MPU_CTRL_ENABLE;
    __asm__ volatile ("dsb");
    __asm__ volatile ("isb");
}