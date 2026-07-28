#include "exec.h"
#include "define.h"
#include "kernel.h"
#include "sddrv.h"
#include "consdrv.h"
#include "lib.h"

typedef struct{
    picox_func_t entry;
} app_param_t;

int exec_app(int argc, char *argv[]){
    app_param_t *param;
    param = (app_param_t *)argv;
    param->entry(0, NULL);
    picox_send(MSGBOX_ID_APPEND, 0, NULL); //アプリケーション実行終了を知らせる
    return 0;
}

int exec_main(int argc, char *argv[]){
    int size;
    char *filename, *load_result_msg;
    uint32_t entry_address;
    picox_func_t entry;
    int load_result;
    app_param_t *param;

    while(1){
        size = 0;
        filename = NULL;
        // shellから実行するファイル名を受け取る
        picox_recv(MSGBOX_ID_APPREQUEST, &size, &filename);
        // 実際のSDカード読み出しとELFロードはsddrvスレッドが行う(メッセージ通信で処理を渡す)
        load_result = sddrv_load_elf(filename, &entry_address);
        if(load_result < 0){ //ロード失敗
            load_result_msg = sddrv_error(load_result);
            consdrv_write("Load failed: ");
            consdrv_write(load_result_msg);
            consdrv_write("\n");
            // runコマンド実行終了をshellへ知らせる
            picox_send(MSGBOX_ID_CMDEND, 0, NULL);
            continue;
        }
        param = picox_malloc(sizeof(app_param_t));
        param->entry = (picox_func_t)entry_address;
        picox_run(exec_app, filename, 8, 0x1000, 0, (char **)param); //ラッパースレッド作成
        picox_free(filename);
        picox_free(param);
        picox_recv(MSGBOX_ID_APPEND, 0, NULL);
        picox_send(MSGBOX_ID_CMDEND, 0, NULL);
    }

    return 0;
}
