#include "define.h"
#include "shell.h"
#include "kernel.h"
#include "exec.h"
#include "consdrv.h"
#include "sddrv.h"

int start_thread(int argc, char *argv[]){
    picox_run(consdrv_main, "consdrv", 1, 0x800, 0, NULL);
    picox_run(sddrv_main,  "sddrv",  2, 0x1000, 0, NULL);
    picox_run(shell_main,  "shell",  3, 0x800, 0, NULL);
    picox_run(exec_main,    "app",    3, 0x1000, 0, NULL);

    picox_chpri(15);
    while(1){
        __asm__ volatile ("wfi");
    }
    return 0;
}