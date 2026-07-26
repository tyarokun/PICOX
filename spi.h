#ifndef SPI_H
#define SPI_H

#include "define.h"

/*
 * RP2040 SPI0 ハードウェアライブラリ。
 * SPI0、GPIO、RESET、SIOレジスタへ直接アクセスするのは spi.c だけとする。
 */
int spi_init(void);
int spi_set_speed(uint32_t divider);

void spi_cs_high(void);
void spi_cs_low(void);

int spi_is_send_enable(void);
void spi_send_byte(uint8_t value);
int spi_is_recv_enable(void);
uint8_t spi_recv_byte(void);
int spi_is_busy(void);

/* 1バイト送信し、同時に受信した1バイトを返す。 */
uint8_t spi_transfer_byte(uint8_t value);

#endif
