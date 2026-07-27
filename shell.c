#include "define.h"
#include "handler.h"
#include "interrupt.h"
#include "kernel.h"
#include "serial.h"
#include "lib.h"
#include "shell.h"
#include "app.h"
#include "consdrv.h"

/* Cortex-M0+ Application Interrupt and Reset Control Register */
#define SCB_AIRCR                  (*(volatile uint32_t *)0xE000ED0C)
#define SCB_AIRCR_VECTKEY          (0x5FA << 16)
#define SCB_AIRCR_SYSRESETREQ      (1 << 2)

typedef int (*command_func_t)(char *argument);

// コマンドテーブル
typedef struct {
    char *name;   //コマンド名
    command_func_t function;    //実行する関数
    char *description;    //説明文(help用)
} command_entry;

//コマンド前の空白を読み飛ばす
static char *skip_spaces(char *p){
    while (*p == ' ') {
        p++;
    }
    return p;
}

// コマンドの関数宣言
static int command_help(char *argument);
static int command_echo(char *argument);
static int command_version(char *argument);
static int command_clear(char *argument);
static int command_reset(char *argument);
static int command_run(char *argument);

//コマンドテーブル
static command_entry commands[] = {
    {"help",    command_help,       "show this help"},
    {"echo",    command_echo,       "print text"},
    {"version", command_version,    "show kernel version"},
    {"clear",   command_clear,      "clear terminal"},
    {"reset",   command_reset,      "reset the system"},
    {"run",     command_run,        "load and run the program"},
};

#define COMMAND_COUNT ((int)(sizeof(commands) / sizeof(commands[0]))) //コマンドの数

// コマンド処理
static int command_help(char *argument){
    int i;
    (void)argument;
    consdrv_write("commands:\n");
    for (i = 0; i < COMMAND_COUNT; i++) {
        consdrv_write("  ");
        consdrv_write(commands[i].name);
        consdrv_write(" - ");
        consdrv_write(commands[i].description);
        consdrv_write("\n");
    }
    return 0;
}

static int command_echo(char *argument){
    consdrv_write(argument);
    consdrv_write("\n");
    return 0;
}

static int command_version(char *argument){
    (void)argument;
    consdrv_write("PicoX 0.3 (RP2040)\n");
    return 0;
}

static int command_clear(char *argument){
    (void)argument;
    consdrv_write("\033[2J\033[H"); //\033[2J→画面を消去, \033[H→カーソルを左上へ移動
    return 0;
}

static int command_reset(char *argument){
    INTR_DISABLE();
    __asm__ volatile ("dsb"); //リセット要求より前に実行したメモリアクセスが完了するまで待つ
    SCB_AIRCR = SCB_AIRCR_VECTKEY | SCB_AIRCR_SYSRESETREQ;
    __asm__ volatile ("dsb"); //AIRCRへの書き込みを完了させる
    while(1){ //リセットが反映されるまで後続処理を実行しない
        __asm__ volatile ("nop");
    }
}

static int command_run(char *argument){
    int request_size, result_size;
    char *filename, *result;
    if(argument == NULL || *argument == '\0'){
        consdrv_write("usage: run FILE.ELF\n");
        return -1;
    }
    request_size = strlen(argument) + 1;
    filename = (char *)picox_malloc(request_size);
    memcpy(filename, argument, request_size);
    picox_send(MSGBOX_ID_APPREQUEST, request_size, filename); //appスレッドへ要求
    consdrv_write("run request sent\n");
    //runコマンドの実行結果を受け取るまでまつ
    picox_recv(MSGBOX_ID_APPRESULT, &result_size, &result);
    consdrv_write("Load failed: ");
    consdrv_write(result);
    consdrv_write("\n");
    picox_free(result);
    return 0;
}

//入力行からコマンドと引数を分ける
static int command_execute(char *line){
    char *command;
    char *argument;
    int i;
    command = skip_spaces(line);
    if(*command == '\0'){ //Enterだけの時
        return 0;
    }
    argument = command;
    while(*argument && *argument != ' '){ //コマンドより後ろの最初の空白まで飛ばす
        argument++;
    }
    if(*argument){
        *argument++ = '\0'; //空白を'\0'に置き換える (command = "echo", argument = "Hello World" のようになる)
        argument = skip_spaces(argument);
    }
    for(i = 0; i < COMMAND_COUNT; i++){
        if(strcmp(command, commands[i].name) == 0){
            return commands[i].function(argument);
        }
    }
    consdrv_write("unknown command: ");
    consdrv_write(command);
    consdrv_write("\n");
    return -1;
}

int shell_main(int argc, char *argv[]){
    char *line;
    int size;

    consdrv_write("\nPICOX shell started\n");
    while(1){
        consdrv_write("picox> ");
        picox_recv(MSGBOX_ID_CONSRX, &size, &line);
        command_execute(line); //コマンド解析
        picox_free(line);
    }

    return 0;
}