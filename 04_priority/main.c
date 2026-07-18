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
  int a = 0;
  while (1) {
    serial_put_int(a);
    serial_puts("\n");
    a++;
    for(int i = 0; i < 1000000; i++);
    picox_wait();
  }
}

int test_main_2(int argc, char *argv[]){
  serial_puts("test_main_2 started\n");
  int a = 0;
  while (1) {
    serial_put_int(a);
    serial_puts("\n");
    a++;
    for(int i = 0; i < 1000000; i++);
    picox_wait();
  }
}

int start_thread(int argc, char *argv[]){
  picox_run(test_main_0, "test_main_0", 1, 0x100, 0, NULL);
  serial_puts("1\n");
  picox_run(test_main_1, "test_main_1", 1, 0x100, 0, NULL);
  serial_puts("2\n");
  picox_run(test_main_2, "test_main_2", 1, 0x100, 0, NULL);
  serial_puts("3\n");
  picox_chpri(15);
  return 0;
}