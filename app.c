#include "app.h"
#include "define.h"
#include "kernel.h"
#include "sddrv.h"
#include "consdrv.h"
#include "lib.h"

int app_main(int argc, char *argv[]){
    int size, load_result_msg_size;
    char *filename;
    uint32_t entry_address;
    picox_func_t entry;
    int load_result;
    char *load_result_msg, *load_result_msg_tmp;

    while(1){
        size = 0;
        filename = NULL;
        load_result_msg = NULL;
        // shellから実行するファイル名を受け取る
        picox_recv(MSGBOX_ID_APPREQUEST, &size, &filename);
        // 実際のSDカード読み出しとELFロードはsddrvスレッドが行う(メッセージ通信で処理を渡す)
        load_result = sddrv_load_elf(filename, &entry_address);
        if(filename != NULL){
            picox_free(filename);
        }
        if(load_result < 0){ //ロード失敗
            load_result_msg_tmp = sddrv_error(load_result);
            load_result_msg_size = strlen(load_result_msg) + 1;
            load_result_msg = picox_malloc(load_result_msg_size);
            strcpy(load_result_msg_tmp, load_result_msg);
            picox_send(MSGBOX_ID_APPRESULT, load_result_msg_size, load_result_msg);
            continue;
        }
        entry = (picox_func_t)entry_address;
        /* ロード完了後のアプリコードはappスレッドの文脈で実行する。 */
        entry(0, NULL);
    }

    return 0;
}
