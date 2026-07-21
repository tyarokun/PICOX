#include "define.h"
#include "shell.h"
#include "kernel.h"

int start_thread(int argc, char *argv[]){
    picox_thread_id_t shell_id = picox_run(shell_main, "shell", 1, 0x800, 0, NULL);
    picox_chpri(15);
    while(1) __asm__ volatile ("wfi");
    return 0;
}