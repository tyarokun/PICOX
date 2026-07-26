#include "spi.h"

#define REG32(address) (*(volatile uint32_t *)(address))

/* Base addresses */
#define RESETS_BASE     0x4000C000u
#define IO_BANK0_BASE   0x40014000u
#define PADS_BANK0_BASE 0x4001C000u
#define SPI0_BASE       0x4003C000u
#define SIO_BASE        0xD0000000u

/* RESETS */
#define RESETS_RESET      REG32(RESETS_BASE + 0x00u)
#define RESETS_RESET_DONE REG32(RESETS_BASE + 0x08u)

#define RESET_IO_BANK0   (1u << 5)
#define RESET_PADS_BANK0 (1u << 8)
#define RESET_SPI0       (1u << 16)

/* IO_BANK0 / PADS_BANK0 */
#define GPIO_CTRL(gpio) REG32(IO_BANK0_BASE + 0x04u + (gpio) * 8u)
#define GPIO_PAD(gpio)  REG32(PADS_BANK0_BASE + 0x04u + (gpio) * 4u)

#define GPIO_FUNC_SPI 1u
#define GPIO_FUNC_SIO 5u

#define PAD_IE  (1u << 6)
#define PAD_PUE (1u << 3)
#define PAD_PDE (1u << 2)

/* SIO */
#define SIO_GPIO_OUT_SET REG32(SIO_BASE + 0x14u)
#define SIO_GPIO_OUT_CLR REG32(SIO_BASE + 0x18u)
#define SIO_GPIO_OE_SET  REG32(SIO_BASE + 0x24u)

/* SPI0: ARM PL022-compatible register block */
#define SSPCR0  REG32(SPI0_BASE + 0x00u)
#define SSPCR1  REG32(SPI0_BASE + 0x04u)
#define SSPDR   REG32(SPI0_BASE + 0x08u)
#define SSPSR   REG32(SPI0_BASE + 0x0cu)
#define SSPCPSR REG32(SPI0_BASE + 0x10u)

#define SSPCR1_SSE (1u << 1)

#define SSPSR_TNF (1u << 1)
#define SSPSR_RNE (1u << 2)
#define SSPSR_BSY (1u << 4)

/* SPI0 pins */
#define SPI_GPIO_MISO 16u
#define SPI_GPIO_CS   17u
#define SPI_GPIO_SCK  18u
#define SPI_GPIO_MOSI 19u
#define SPI_CS_MASK   (1u << SPI_GPIO_CS)

/*
 * f_spi = clk_peri / (CPSDVSR * (1 + SCR))
 * SCR=1、CPSDVSR=250、clk_peri=125MHz の場合は約250kHz。
 */
#define SPI_INITIAL_DIVIDER 250u

int spi_set_speed(uint32_t divider){
    uint32_t dummy;

    /* PL022のCPSDVSRは2～254の偶数。 */
    if(divider < 2u || divider > 254u || (divider & 1u) != 0u){
        return -1;
    }

    SSPCR1 = 0u;

    /* 設定変更前に受信FIFOを空にする。 */
    while((SSPSR & SSPSR_RNE) != 0u){
        dummy = SSPDR;
        (void)dummy;
    }

    /* SPI mode 0、8bit、SCR=1。 */
    SSPCR0 = (1u << 8) | 7u;
    SSPCPSR = divider;
    SSPCR1 = SSPCR1_SSE;
    return 0;
}

int spi_init(void){
    uint32_t reset_mask =
        RESET_IO_BANK0 | RESET_PADS_BANK0 | RESET_SPI0;

    RESETS_RESET &= ~reset_mask;
    while((RESETS_RESET_DONE & reset_mask) != reset_mask){
    }

    GPIO_CTRL(SPI_GPIO_MISO) = GPIO_FUNC_SPI;
    GPIO_CTRL(SPI_GPIO_SCK) = GPIO_FUNC_SPI;
    GPIO_CTRL(SPI_GPIO_MOSI) = GPIO_FUNC_SPI;
    GPIO_CTRL(SPI_GPIO_CS) = GPIO_FUNC_SIO;

    GPIO_PAD(SPI_GPIO_MISO) =
        (GPIO_PAD(SPI_GPIO_MISO) | PAD_IE | PAD_PUE) & ~PAD_PDE;
    GPIO_PAD(SPI_GPIO_CS) =
        (GPIO_PAD(SPI_GPIO_CS) | PAD_PUE) & ~PAD_PDE;

    /* CSをHighにしてから出力を有効にする。 */
    spi_cs_high();
    SIO_GPIO_OE_SET = SPI_CS_MASK;

    return spi_set_speed(SPI_INITIAL_DIVIDER);
}

void spi_cs_high(void){
    SIO_GPIO_OUT_SET = SPI_CS_MASK;
}

void spi_cs_low(void){
    SIO_GPIO_OUT_CLR = SPI_CS_MASK;
}

int spi_is_send_enable(void){
    return (SSPSR & SSPSR_TNF) != 0u;
}

void spi_send_byte(uint8_t value){
    SSPDR = value;
}

int spi_is_recv_enable(void){
    return (SSPSR & SSPSR_RNE) != 0u;
}

uint8_t spi_recv_byte(void){
    return (uint8_t)SSPDR;
}

int spi_is_busy(void){
    return (SSPSR & SSPSR_BSY) != 0u;
}

uint8_t spi_transfer_byte(uint8_t value){
    while(!spi_is_send_enable()){
    }
    spi_send_byte(value);

    while(!spi_is_recv_enable()){
    }
    return spi_recv_byte();
}
