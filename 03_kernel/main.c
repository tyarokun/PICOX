#include "define.h"
#include "serial.h"
#include "kernel.h"
#include "lib.h"

int test_main_0(int argc, char *argv[])
{
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

int test_main_1(int argc, char *argv[])
{
  serial_puts("test_main_1 started\n");
  return 0;
}

int start_thread(int argc, char *argv[])
{
  picox_run(test_main_0, "command", 0x100, 0, NULL);
  picox_run(test_main_1, "command", 0x100, 0, NULL);
  serial_puts("end start_thread\n");
  return 0;
}