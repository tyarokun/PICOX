#include "app.h"
#include "define.h"
#include "serial.h"
#include "kernel.h"
#include "sd_elf.h"
#include "consdrv.h"

int app_main(int argc, char *argv[]){
    int size;
    char *filename;
    uint32_t entry_address;
    picox_func_t entry;
    int load_result;

    while(1){
        size = 0;
        filename = NULL;
        picox_recv(MSGBOX_ID_APPREQUEST, &size, &filename); //受信待ち(filenameを受け取る)
        load_result = sd_elf_load(filename, &entry_address);
        picox_free(filename);
        if(load_result < 0){
            consdrv_write("Load failed\n");
            continue;
        }
        entry = (picox_func_t)entry_address;
        //アプリスレッドのコンテキストで実行する
        entry(0, NULL);
    }
}