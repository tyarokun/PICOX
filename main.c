#include "define.h"
#include "kernel.h"
#include "consdrv.h"
#include "command.h"

int start_thread(int argc, char *argv[]){
    picox_run(consdrv_main, "consdrv", 1, 0x800, 0, NULL);
    picox_run(command_main, "command", 5, 0x800, 0, NULL);
    picox_chpri(15);
    while(1) __asm__ volatile ("wfi");
    return 0;
}