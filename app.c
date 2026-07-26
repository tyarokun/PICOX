#include "app.h"
#include "define.h"
#include "kernel.h"
#include "sddrv.h"
#include "consdrv.h"

int app_main(int argc, char *argv[]){
    int size;
    char *filename;
    uint32_t entry_address;
    picox_func_t entry;
    int load_result;

    (void)argc;
    (void)argv;

    while(1){
        size = 0;
        filename = NULL;

        // shellから実行するファイル名を受け取る
        picox_recv(MSGBOX_ID_APPREQUEST, &size, &filename);

        // 実際のSDカード読み出しとELFロードはsddrvスレッドが行う
        load_result = sddrv_load_elf(filename, &entry_address);

        if(filename != NULL){
            picox_free(filename);
        }

        if(load_result < 0){
            consdrv_write("Load failed: ");
            consdrv_write((char *)sddrv_error(load_result));
            consdrv_write("\n");
            continue;
        }

        entry = (picox_func_t)entry_address;

        /* ロード完了後のアプリコードはappスレッドの文脈で実行する。 */
        entry(0, NULL);
    }

    return 0;
}
