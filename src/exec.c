#include "exec.h"
#include "define.h"
#include "kernel.h"
#include "sddrv.h"
#include "consdrv.h"
#include "lib.h"

typedef struct{
    picox_func_t entry;
    int background;
} app_param_t;

/*
*ラッパースレッドによってアプリ終了を通知することができるので
*アプリ終了まで待機できる
*/
int exec_app(int argc, char *argv[]){ //ラッパースレッド
    app_param_t *param;
    picox_func_t entry;
    int background;
    param = (app_param_t *)argv;
    entry = param->entry;
    background = param->background;
    picox_free(param);
    entry(0, NULL);
    if(!background){
        picox_send(MSGBOX_ID_APPEND, 0, NULL); //アプリケーション実行終了を通知する
    }
    return 0;
}

int exec_main(int argc, char *argv[]){
    int size;
    char *load_result_msg;
    uint32_t entry_address;
    picox_func_t entry;
    int load_result;
    app_param_t *param;
    exec_request_t *request;

    while(1){
        size = 0;
        // shellからリクエストを受け取る
        picox_recv(MSGBOX_ID_APPREQUEST, &size, (char **)&request);
        // 実際のSDカード読み出しとELFロードはsddrvスレッドが行う(メッセージ通信で処理を渡す)
        load_result = sddrv_load_elf(request->filename, &entry_address);
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
        param->background = request->background;
        picox_run(exec_app, request->filename, 8, 0x1000, 0, (char **)param); //ラッパースレッド作成
        if(request->background){
            picox_send(MSGBOX_ID_CMDEND, 0, NULL);
        }else{
            picox_recv(MSGBOX_ID_APPEND, 0, NULL);
            picox_send(MSGBOX_ID_CMDEND, 0, NULL);
        }
        picox_free(request);
    }

    return 0;
}
