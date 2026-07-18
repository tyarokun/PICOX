#include "define.h"
#include "serial.h"
#include "kernel.h"
#include "lib.h"

int test_11_1(int argc, char *argv[]){
  char *p;
  int size;
  serial_puts("test11_1 started.\n");
  /* 静的領域をメッセージで受信 */
  serial_puts("test11_1 recv in.\n");
  picox_recv(MSGBOX_ID_MSGBOX1, &size, &p); /* 受信 */
  serial_puts("test11_1 recv out.\n");
  serial_puts(p);
  /* 動的に獲得した領域をメッセージで受信 */
  serial_puts("test11_1 recv in.\n");
  picox_recv(MSGBOX_ID_MSGBOX1, &size, &p); /* 受信 */
  serial_puts("test11_1 recv out.\n");
  serial_puts(p);
  picox_free(p); /* メモリ解放 */
  /* 静的領域をメッセージで送信 */
  serial_puts("test11_1 send in.\n");
  picox_send(MSGBOX_ID_MSGBOX2, 15, "static memory\n"); /* 送信 */
  serial_puts("test11_1 send out.\n");
  /* 動的に獲得した領域をメッセージで送信 */
  p = picox_malloc(18); /* メモリ獲得 */
  strcpy(p, "allocated memory\n");
  serial_puts("test11_1 send in.\n");
  picox_send(MSGBOX_ID_MSGBOX2, 18, p); /* 送信 */
  serial_puts("test11_1 send out.\n");
  /* メモリ解放は受信側で行うので，ここでは不要 */
  serial_puts("test11_1 exit.\n");
  return 0;
}

int test_11_2(int argc, char *argv[]){
  char *p;
  int size;
  serial_puts("test11_2 started.\n");
  /* 静的領域をメッセージで送信 */
  serial_puts("test11_2 send in.\n");
  picox_send(MSGBOX_ID_MSGBOX1, 15, "static memory\n"); /* 送信 */
  serial_puts("test11_2 send out.\n");
  /* 動的に獲得した領域をメッセージで送信 */
  p = picox_malloc(18); /* メモリ獲得 */
  strcpy(p, "allocated memory\n");
  serial_puts("test11_2 send in.\n");
  picox_send(MSGBOX_ID_MSGBOX1, 18, p); /* 送信 */
  serial_puts("test11_2 send out.\n");
  /* メモリ解放は受信側で行うので，ここでは不要 */
  /* 静的領域をメッセージで受信 */
  serial_puts("test11_2 recv in.\n");
  picox_recv(MSGBOX_ID_MSGBOX2, &size, &p); /* 受信 */
  serial_puts("test11_2 recv out.\n");
  serial_puts(p);
  /* 動的に獲得した領域をメッセージで受信 */
  serial_puts("test11_2 recv in.\n");
  picox_recv(MSGBOX_ID_MSGBOX2, &size, &p); /* 受信 */
  serial_puts("test11_2 recv out.\n");
  serial_puts(p);
  picox_free(p); /* メモリ解放 */
  serial_puts("test11_2 exit.\n");
  return 0;
}

int start_thread(int argc, char *argv[]){
  picox_run(test_11_1, "test_11_1", 1, 0x100, 0, NULL);
  picox_run(test_11_2, "test_11_2", 1, 0x100, 0, NULL);
  picox_chpri(15);
  return 0;
}