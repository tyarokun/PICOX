#include "exec.h"
#include "define.h"
#include "kernel.h"
#include "sddrv.h"
#include "consdrv.h"
#include "lib.h"

typedef struct{
    elf_loaded_image image;
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
    entry = (picox_func_t)param->image.entry;
    background = param->background;
    entry(0, NULL);
    elf_loader_unload(&param->image);
    if(!background){
        picox_send(MSGBOX_ID_APPEVENT, 0, NULL); //アプリケーション実行終了を通知する
    }
    picox_free(param);
    return 0;
}

int exec_main(int argc, char *argv[]){
    int size;
    char *load_result_msg;
    int load_result;
    app_param_t *param;
    exec_request_t *request;
    elf_loaded_image image;
    picox_thread_id_t id;
    char *message;

    while(1){
        size = 0;
        // shellからリクエストを受け取る
        picox_recv(MSGBOX_ID_APPREQUEST, &size, (char **)&request);
        memset(&image, 0, (long)sizeof(image));
        // 実際のSDカード読み出しとELFロードはsddrvスレッドが行う(メッセージ通信で処理を渡す)
        load_result = sddrv_load_elf(request->filename, &image);
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
        param->image = image;
        param->background = request->background;
        id = picox_run(exec_app, request->filename, 8, 0x1000, 0, (char **)param); //ラッパースレッド作成
        if(request->background){
            picox_send(MSGBOX_ID_CMDEND, 0, NULL);
        }else{
            picox_recv(MSGBOX_ID_APPEVENT, &size, &message); // アプリ終了やCtrl-Cによってメッセージ受け取り
            picox_kill(id);
            picox_send(MSGBOX_ID_CMDEND, 0, NULL);
        }
        picox_free(request);
    }

    return 0;
}
