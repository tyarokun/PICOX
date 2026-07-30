#include "define.h"
#include "kernel.h"
#include "spi.h"
#include "fat32.h"
#include "elf_loader.h"
#include "sddrv.h"

#define CMD0   0u
#define CMD8   8u
#define CMD16 16u
#define CMD17 17u
#define CMD55 55u
#define CMD58 58u
#define ACMD41 41u

#define SD_INITIAL_CLOCK_BYTES 10u
#define SD_COMMAND_WAIT_COUNT  16u
#define SD_TOKEN_WAIT_COUNT    200000u

/* SCR=1、clk_peri=125MHzの場合。 */
#define SD_INITIAL_DIVIDER  250u /* 約250kHz */
#define SD_TRANSFER_DIVIDER 4u   /* 約15.6MHz */

typedef enum {
    SDDRV_REQUEST_LOAD_ELF = 0
} sddrv_request_type;

typedef struct {
    sddrv_request_type type;
    const char *filename;
    elf_loaded_image image;
    int result;
} sddrv_request;

/* SDカードとFAT32の状態はsddrvスレッドだけが所有する。 */
static int sd_block_addressing;
static fat32_fs filesystem;

static void sd_transfer(const uint8_t *tx, uint8_t *rx, uint32_t length, uint8_t fill){
    uint32_t i;
    uint8_t received;

    for(i = 0u; i < length; i++){
        received = spi_transfer_byte(tx != NULL ? tx[i] : fill);
        if(rx != NULL){
            rx[i] = received;
        }
    }
}

static int sd_wait_while(uint8_t tx, uint8_t mask, uint8_t value, uint32_t limit, uint8_t *received){
    uint32_t i;
    uint8_t current = 0xffu;

    if(received == NULL || limit == 0u){
        return -1;
    }

    for(i = 0u; i < limit; i++){
        current = spi_transfer_byte(tx);
        if((current & mask) != value){
            *received = current;
            return 0;
        }
    }

    *received = current;
    return -1;
}

static void sd_wait_idle(void){
    while(spi_is_busy()){
    }
}

static void sd_deselect(void){
    spi_cs_high();
    (void)spi_transfer_byte(0xffu);
}

/* 6バイトのSDコマンドを送信し、R1レスポンスを返す。 */
static int sd_command(uint8_t command, uint32_t argument, uint8_t *response){
    uint8_t packet[6];

    if(response == NULL){
        return -1;
    }

    packet[0] = (uint8_t)(0x40u | command);
    packet[1] = (uint8_t)(argument >> 24);
    packet[2] = (uint8_t)(argument >> 16);
    packet[3] = (uint8_t)(argument >> 8);
    packet[4] = (uint8_t)argument;
    packet[5] = 0x01u;

    if(command == CMD0){
        packet[5] = 0x95u;
    }else if(command == CMD8){
        packet[5] = 0x87u;
    }

    sd_deselect();
    spi_cs_low();
    (void)spi_transfer_byte(0xffu);
    sd_transfer(packet, NULL, (uint32_t)sizeof(packet), 0xffu);

    /* R1のbit7が0になるまで待つ。 */
    if(sd_wait_while(0xffu, 0x80u, 0x80u, SD_COMMAND_WAIT_COUNT, response) < 0){
        sd_deselect();
        return -1;
    }

    return 0;
}

static int sd_card_init(void){
    uint8_t response;
    uint8_t r7[4];
    uint8_t ocr[4];
    uint32_t i;
    int version2 = 0;
    sd_block_addressing = 0;
    sd_wait_idle();
    if(spi_set_speed(SD_INITIAL_DIVIDER) < 0){
        return -1;
    }
    /* CSをHighにした状態で80クロック以上送る。 */
    spi_cs_high();
    sd_transfer(NULL, NULL, SD_INITIAL_CLOCK_BYTES, 0xffu);
    /* CMD0: SDカードをアイドル状態へ移行する。 */
    response = 0xffu;
    for(i = 0u; i < 10u; i++){
        if(sd_command(CMD0, 0u, &response) < 0){
            return -1;
        }
        sd_deselect();
        if(response == 0x01u){
            break;
        }
    }
    if(response != 0x01u){
        return -1;
    }
    /* CMD8: Version 2以降と対応電圧を確認する。 */
    if(sd_command(CMD8, 0x1aau, &response) < 0){
        return -1;
    }
    if(response == 0x01u){
        sd_transfer(NULL, r7, (uint32_t)sizeof(r7), 0xffu);
        sd_deselect();
        if(r7[2] != 0x01u || r7[3] != 0xaau){
            return -1;
        }
        version2 = 1;
    }else if((response & 0x04u) != 0u){
        sd_deselect();
    }else{
        sd_deselect();
        return -1;
    }
    /* CMD55 + ACMD41: 初期化完了まで待つ。 */
    response = 0xffu;
    for(i = 0u; i < 5000u; i++){
        if(sd_command(CMD55, 0u, &response) < 0){
            return -1;
        }
        sd_deselect();
        if(response > 0x01u){
            continue;
        }
        if(sd_command(ACMD41,
                      version2 ? 0x40000000u : 0u,
                      &response) < 0){
            return -1;
        }
        sd_deselect();
        if(response == 0u){
            break;
        }
    }
    if(response != 0u){
        return -1;
    }
    /* CMD58: OCRを読み、ブロックアドレッシングか確認する。 */
    if(sd_command(CMD58, 0u, &response) < 0){
        return -1;
    }
    if(response != 0u){
        sd_deselect();
        return -1;
    }
    sd_transfer(NULL, ocr, (uint32_t)sizeof(ocr), 0xffu);
    sd_deselect();
    if(version2 && (ocr[0] & 0x40u) != 0u){
        sd_block_addressing = 1;
    }else{
        if(sd_command(CMD16, FAT32_SECTOR_SIZE, &response) < 0){
            return -1;
        }
        sd_deselect();
        if(response != 0u){
            return -1;
        }
    }
    sd_wait_idle();
    return spi_set_speed(SD_TRANSFER_DIVIDER);
}

/*
 * FAT32から呼ばれるセクタ読み出し関数
 * fat32_mount/readはsddrv_mainの処理中に呼ばれるため、実行主体もsddrvスレッド
 */
static int sd_read_sector(uint32_t lba, uint8_t *buffer, void *context){
    uint32_t argument;
    uint8_t response;
    uint8_t token = 0xffu;

    (void)context;

    if(buffer == NULL){
        return -1;
    }

    argument = sd_block_addressing ? lba : lba * FAT32_SECTOR_SIZE;

    if(sd_command(CMD17, argument, &response) < 0){
        return -1;
    }
    if(response != 0u){
        sd_deselect();
        return -1;
    }

    /* 0xff以外のデータトークンが返るまで待つ。 */
    if(sd_wait_while(0xffu, 0xffu, 0xffu, SD_TOKEN_WAIT_COUNT, &token) < 0 || token != 0xfeu){
        sd_deselect();
        return -1;
    }

    sd_transfer(NULL, buffer, FAT32_SECTOR_SIZE, 0xffu);
    sd_transfer(NULL, NULL, 2u, 0xffu); /* CRCを読み捨てる。 */
    sd_deselect();
    return 0;
}

static int sddrv_load_elf_execute(const char *filename, elf_loaded_image *image){
    fat32_file file;
    int result;
    if(filename == NULL || image == NULL){
        return SDDRV_ERR_REQUEST;
    }
    if(sd_card_init() < 0){
        return SDDRV_ERR_SD_INIT;
    }
    if(fat32_mount(&filesystem, sd_read_sector, NULL) < 0){
        return SDDRV_ERR_FAT32;
    }
    result = fat32_open(&filesystem, filename, &file);
    if(result == FAT32_OPEN_INVALID_NAME){
        return SDDRV_ERR_FILENAME;
    }
    if(result == FAT32_OPEN_NOT_FOUND){
        return SDDRV_ERR_NOT_FOUND;
    }
    if(result < 0){
        return SDDRV_ERR_SD_READ;
    }
    result = elf_loader_load(&file, image);
    switch(result){
        case ELF_LOADER_OK:
            return 0;
        case ELF_LOADER_ERR_HEADER:
            return SDDRV_ERR_ELF_HEADER;
        case ELF_LOADER_ERR_FORMAT:
            return SDDRV_ERR_ELF_FORMAT;
        case ELF_LOADER_ERR_RANGE:
            return SDDRV_ERR_ELF_RANGE;
        case ELF_LOADER_ERR_READ:
            return SDDRV_ERR_ELF_READ;
        case ELF_LOADER_ERR_NO_SEGMENT:
            return SDDRV_ERR_NO_SEGMENT;
        default:
            return SDDRV_ERR_ELF_READ;
    }
}

static int sddrv_call(sddrv_request *request){
    int size = 0;
    char *response = NULL;
    if(request == NULL){
        return SDDRV_ERR_REQUEST;
    }
    if(picox_send(MSGBOX_ID_SDREQUEST, (int)sizeof(*request), (char *)request) < 0){
        return SDDRV_ERR_REQUEST;
    }
    picox_recv(MSGBOX_ID_SDRESULT, &size, &response);
    if(response != (char *)request || size != (int)sizeof(*request)){
        return SDDRV_ERR_REQUEST;
    }
    return request->result;
}

int sddrv_load_elf(const char *filename, elf_loaded_image *image){
    sddrv_request request = {0};
    int result;
    if(filename == NULL || image == NULL){
        return SDDRV_ERR_REQUEST;
    }
    request.type = SDDRV_REQUEST_LOAD_ELF;
    request.filename = filename;
    result = sddrv_call(&request);
    if(result == 0){
        *image = request.image;
    }
    return result;
}

int sddrv_main(int argc, char *argv[]){
    int size;
    char *message;
    sddrv_request *request;

    spi_init();

    while(1){
        size = 0;
        message = NULL;
        picox_recv(MSGBOX_ID_SDREQUEST, &size, &message);
        if(message == NULL || size != (int)sizeof(sddrv_request)){
            continue;
        }
        request = (sddrv_request *)message;
        switch(request->type){
            case SDDRV_REQUEST_LOAD_ELF:
                request->result = sddrv_load_elf_execute(request->filename, &request->image);
                break;
            default:
                request->result = SDDRV_ERR_REQUEST;
                break;
        }
        picox_send(MSGBOX_ID_SDRESULT, (int)sizeof(*request), (char *)request);
    }

    return 0;
}

const char *sddrv_error(int error){
    switch(error){
        case SDDRV_ERR_SD_INIT:     return "SD init failed";
        case SDDRV_ERR_SD_READ:     return "SD read failed";
        case SDDRV_ERR_FAT32:       return "FAT32 mount failed";
        case SDDRV_ERR_NOT_FOUND:   return "file not found";
        case SDDRV_ERR_ELF_HEADER:  return "invalid ELF header";
        case SDDRV_ERR_ELF_FORMAT:  return "unsupported ELF";
        case SDDRV_ERR_ELF_RANGE:   return "ELF is outside app RAM";
        case SDDRV_ERR_ELF_READ:    return "ELF read failed";
        case SDDRV_ERR_NO_SEGMENT:  return "no PT_LOAD segment";
        case SDDRV_ERR_FILENAME:    return "invalid FAT 8.3 filename";
        case SDDRV_ERR_REQUEST:     return "invalid SD driver request";
        case SDDRV_ERR_NO_MEMORY:   return "no free app RAM";
        default:                    return "unknown error";
    }
}