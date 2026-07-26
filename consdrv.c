#include "define.h"
#include "serial.h"
#include "kernel.h"
#include "handler.h"
#include "consdrv.h"
#include "lib.h"

#define SHELL_RECV_BUFFER_SIZE 48

#define SHELL_SEND_BUFFER_SIZE 512
#define SHELL_SEND_BUFFER_MASK (SHELL_SEND_BUFFER_SIZE - 1) //リングバッファにするためのインデックス用 (512 & 511 = 1000000000 & 111111111 = 0000000000)

//シェルの受信・送信バッファの保持
typedef struct {
    char recv_buffer[SHELL_RECV_BUFFER_SIZE];
    int recv_length;
    int previous_was_cr; //直前の文字が改行だったかを記録する

    char send_buffer[SHELL_SEND_BUFFER_SIZE];
    volatile int send_head;
    volatile int send_tail;
} console_device;

static console_device console;

// 送信リングバッファの空き容量を求める
static int send_buffer_free(console_device *cons){
    int send_buffer_used = cons->send_tail - cons->send_head & SHELL_SEND_BUFFER_MASK; //インデックス計算
    return (SHELL_SEND_BUFFER_SIZE - 1) - send_buffer_used; //空き容量を計算
}

// 1バイトを送信バッファへ追加する
static int send_push_raw(console_device *cons, char value){
    int next = (cons->send_tail + 1) & SHELL_SEND_BUFFER_MASK;
    if(next == cons->send_head){
        return -1;
    }
    cons->send_buffer[cons->send_tail] = value;
    cons->send_tail = next;
    return 0;
}

//必要な送信バッファ量を計算する
static int send_required_size(char *data, int length){
    int i;
    int required = 0;
    for (i = 0; i < length; i++) {
        if(data[i] == '\n'){ //改行の時は2バイト必要
            required += 2;
        }else{
            required += 1;
        }
    }
    return required;
}

// 文字列をリングバッファへ追加する
static int send_push_text(console_device *cons, char *data, int length){
    int i;
    int required;
    required = send_required_size(data, length);
    if((int)required > send_buffer_free(cons)) {
        return -1;
    }
    for(i = 0; i < length; i++){
        if(data[i] == '\n'){
            send_push_raw(cons, (char)'\r');
        }
        send_push_raw(cons, (char)data[i]);
    }
    return 0;
}

//送信バッファからUARTへ送る
static void send_drain(console_device *cons){
    while(cons->send_head != cons->send_tail && serial_is_send_enable()){
        serial_send_byte(cons->send_buffer[cons->send_head]);
        cons->send_head = (cons->send_head + 1) & SHELL_SEND_BUFFER_MASK; //headを1進める
    }
    if (cons->send_head == cons->send_tail){ //送信待ちがなくなったので送信割り込みを無効にする
        serial_intr_send_disable();
    } else if (!serial_intr_is_send_enable()){ //UART FIFOが満杯になった時
        serial_intr_send_enable();  //UARTの送信FIFOに空きができたときに続きが送れるよう、送信割り込みを有効にする
    }
}

// シェルスレッドから文字列を送る
static int send_text(console_device *cons, char *data, int length){
    int result;
    INTR_DISABLE();
    result = send_push_text(cons, data, length);
    if (result == 0) {
        send_drain(cons);
    }
    INTR_ENABLE();
    return result;
}

// UART割り込み内の処理から文字列を送る
static void send_text_from_interrupt(console_device *cons, char *data, int length){
    if(send_push_text(cons, data, length) == 0){
        send_drain(cons);
    }
}

// 入力された1行をメッセージとして送る (Enterが押された時に実行される)
static void send_input_line(console_device *cons){
    char *message;
    int size = cons->recv_length + 1; //末尾の'\0'の分 +1 する
    message = (char *)picoxs_malloc(size);
    memcpy(message, cons->recv_buffer, cons->recv_length);
    message[cons->recv_length] = '\0'; //末尾に'\0'を入れる
    picoxs_send(MSGBOX_ID_CONSRX, size, message);
    cons->recv_length = 0;
}

// 受信した1文字を処理する
static void process_received_character(console_device *cons, char c){
    if(c == '\n' && cons->previous_was_cr){ //CRLFのLFを無視(Enterが "\r\n" で届いた場合、先に '\r' をEnterとして処理済み)
        cons->previous_was_cr = 0;
        return;
    }
    if(c == '\r'){ //CRを受信した場合、内部ではLFとして扱う
        cons->previous_was_cr = 1;
        c = '\n';
    }else{
        cons->previous_was_cr = 0;
    }
    if(c == '\n'){ //Enterの処理
        send_text_from_interrupt(cons, "\n", 1);
        send_input_line(cons); //入力された1行の処理を開始(シェルへ送信)
        return;
    }
    if(c == 0x03){ //Ctrl-Cの処理
        cons->recv_length = 0;
        send_text_from_interrupt(cons, "^C\n", 3);
        send_input_line(cons);
        return;
    }
    if(c == '\b' || c == 0x7f){ // Backspace と Delete
        if (cons->recv_length > 0) {
            cons->recv_length--;
            send_text_from_interrupt(cons, "\b \b", 3); //カーソル1文字戻す→空白→カーソル1文字戻す
        }
        return;
    }
    if(c >= ' ' && c <= '~'){ // 入力された文字がASCIIの範囲内か
        if (cons->recv_length < SHELL_RECV_BUFFER_SIZE - 1) {
            char character = c;
            cons->recv_buffer[cons->recv_length++] = character;
            send_text_from_interrupt(cons, &character, 1);
        }else{
            char bell = '\a';
            send_text_from_interrupt(cons, &bell, 1);
        }
    }
}

// ソフトウェア割り込みハンドラ
static void consdrv_interrupt(void){
    if(serial_intr_is_recv() || serial_is_recv_enable()){ //受信割り込みが発生したか、UARTの受信FIFOに文字が残っている場合
        while(serial_is_recv_enable()){
            process_received_character(&console, serial_recv_byte());
        }
        serial_intr_clear_recv(); //送信割り込みフラグをクリア
    }
    if(serial_intr_is_send()) { // UART送信FIFOに空きができたことによる割り込みなら、フラグをクリア
        serial_intr_clear_send();
    }
    if(console.send_head != console.send_tail){ //送信リングバッファにデータが残っていれば、続きを送る
        send_drain(&console);
    }else{ //残っていなければ送信割り込みを止める
        serial_intr_send_disable();
    }
}

// 外部から使用される唯一のライブラリ関数
int consdrv_write(char *text){
    char *message;
    int length;
    length = strlen(text);
    message = (char *)picox_malloc(length);
    memcpy(message, text, length);
    picox_send(MSGBOX_ID_CONSTX, length, message);
}

// コンソールドライバのスレッド
int consdrv_main(int argc, char *argv[]){
    int size;
    char *message;
    memset(&console, 0, sizeof(console));
    serial_intr_recv_disable();
    serial_intr_send_disable();
    picox_setintr(SOFTVEC_TYPE_SERINTR, consdrv_interrupt);
    serial_interrupt_enable();
    serial_intr_recv_enable();

    while(1){
        size = 0;
        message = NULL;
        picox_recv(MSGBOX_ID_CONSTX, &size, &message);
        if(message != NULL && size > 0){
            send_text(&console, message, size); //コンソールへ表示
        }
        if(message != NULL){
            picox_free(message);
        }
    }
}