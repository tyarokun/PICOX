#include "app.h"
#include "define.h"
#include "kernel.h"
#include "sddrv.h"
#include "consdrv.h"
#include "lib.h"

int app_main(int argc, char *argv[]){
    int size;
    char *filename, *load_result_msg;
    uint32_t entry_address;
    picox_func_t entry;
    int load_result;

    while(1){
        size = 0;
        filename = NULL;
        // shellから実行するファイル名を受け取る
        picox_recv(MSGBOX_ID_APPREQUEST, &size, &filename);
        // 実際のSDカード読み出しとELFロードはsddrvスレッドが行う(メッセージ通信で処理を渡す)
        load_result = sddrv_load_elf(filename, &entry_address);
        if(filename != NULL){
            picox_free(filename);
        }
        if(load_result < 0){ //ロード失敗
            load_result_msg = sddrv_error(load_result);
            consdrv_write("Load failed: ");
            consdrv_write(load_result_msg);
            consdrv_write("\n");
            // runコマンド実行終了をshellへ知らせる
            picox_send(MSGBOX_ID_CMDEND, 0, NULL);
            continue;
        }
        entry = (picox_func_t)entry_address;
        /* ロード完了後のアプリコードはappスレッドの文脈で実行する。 */
        entry(0, NULL);
    }

    return 0;
}
