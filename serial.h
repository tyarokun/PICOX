#ifndef SERIAL_H
#define SERIAL_H

#include "define.h"

/* KOZOS第7章のAPIをRP2040 UART0向けに移植した低レベルドライバ。 */
void serial_init(void);                       /* デバイス初期化 */
int serial_is_send_enable(void);             /* 送信可能か */
void serial_send_byte(unsigned char c); /* 1文字送信 */
int serial_is_recv_enable(void);             /* 受信可能か */
unsigned char serial_recv_byte(void);        /* 1文字受信 */

int serial_intr_is_send_enable(void); /* 送信割込み有効か */
void serial_intr_send_enable(void);   /* 送信割込み有効化 */
void serial_intr_send_disable(void);  /* 送信割込み無効化 */
int serial_intr_is_recv_enable(void); /* 受信割込み有効か */
void serial_intr_recv_enable(void);   /* 受信割込み有効化 */
void serial_intr_recv_disable(void);  /* 受信割込み無効化 */

/* OS起動前と致命的エラー表示で使用するポーリング出力。 */
void serial_putc(char c);
void serial_puts(const char *s);
void serial_put_uint(uint32_t value);
void serial_put_int(int value);
void serial_put_hex(uint32_t value);

#endif