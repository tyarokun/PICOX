#ifndef SDDRV_H
#define SDDRV_H

#include "define.h"
#include "elf_loader.h"

#define SDDRV_ERR_SD_INIT     -1
#define SDDRV_ERR_SD_READ     -2
#define SDDRV_ERR_FAT32       -3
#define SDDRV_ERR_NOT_FOUND   -4
#define SDDRV_ERR_ELF_HEADER  -5
#define SDDRV_ERR_ELF_FORMAT  -6
#define SDDRV_ERR_ELF_RANGE   -7
#define SDDRV_ERR_ELF_READ    -8
#define SDDRV_ERR_NO_SEGMENT  -9
#define SDDRV_ERR_FILENAME   -10
#define SDDRV_ERR_REQUEST    -11
#define SDDRV_ERR_NO_MEMORY  -12

// SDカード、FAT32、ELFロードを一元管理するドライバスレッド
int sddrv_main(int argc, char *argv[]);

/*
 * appスレッドからsddrvスレッドへELFロード要求を送り、完了まで待つ
 * SDカードを実際に読み出すのはsddrvスレッドだけ
 */
int sddrv_load_elf(const char *filename, elf_loaded_image *image);

const char *sddrv_error(int error);

#endif
