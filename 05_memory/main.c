#include "define.h"
#include "serial.h"
#include "kernel.h"
#include "lib.h"

int test_main_0(int argc, char *argv[]){
  char buf[32];
  (void)argc;
  (void)argv;
  serial_puts("test_main_0 started.\n");
  while(1){
    serial_puts("> ");
    serial_gets(buf, sizeof(buf));
    if (!strncmp(buf, "echo", 4)){
      serial_puts(buf + 4);
      serial_puts("\n");
    }else if(!strcmp(buf, "exit")){
      break;
    }else{
      serial_puts("unknown.\n");
    }
  }
  serial_puts("test_main_0 exit.\n");
  return 0;
}

int test_main_1(int argc, char *argv[]){
  serial_puts("test_main_1 started\n");
  char *p1, *p2;
  int i, j;

  for (i = 4; i <= 56; i += 4) {
    /* メモリを動的に獲得 */
    p1 = picox_malloc(i);
    p2 = picox_malloc(i);

    for (j = 0; j < i - 1; j++) {
      p1[j] = 'a';
      p2[j] = 'b';
    }
    p1[j] = '\0';
    p2[j] = '\0';

    serial_put_hex((uint32_t)p1); serial_puts(" "); serial_puts(p1); serial_puts("\n");
    serial_put_hex((uint32_t)p2); serial_puts(" "); serial_puts(p2); serial_puts("\n");

    /* メモリ解放 */
    picox_free(p1);
    picox_free(p2);
  }
  return 0;
}

int start_thread(int argc, char *argv[]){
  picox_run(test_main_0, "test_main_0", 1, 0x100, 0, NULL);
  picox_run(test_main_1, "test_main_1", 1, 0x100, 0, NULL);
  picox_chpri(15);
  return 0;
}