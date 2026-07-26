#ifndef FAT32_H
#define FAT32_H

#include "define.h"

#define FAT32_SECTOR_SIZE 512u

#define FAT32_OPEN_NOT_FOUND     1
#define FAT32_OPEN_IO_ERROR     -1
#define FAT32_OPEN_INVALID_NAME -2

/*
 * FAT32がセクタを必要としたときに呼び出す関数
 * 実際のSDカード読み出し関数はsddrv.c内にだけ置く
 */
typedef int (*fat32_read_sector_func_t)(uint32_t lba, uint8_t *buffer, void *context);

/* FAT32のマウント状態 sddrvスレッドがこの構造体を所有する */
typedef struct {
    uint32_t fat_start_lba;
    uint32_t data_start_lba;
    uint32_t root_cluster;
    uint32_t sectors_per_cluster;

    fat32_read_sector_func_t read_sector;
    void *read_context;

    uint8_t sector_buffer[FAT32_SECTOR_SIZE];
} fat32_fs;

/* 開いたファイル どのFAT32状態に属するかも保持する */
typedef struct {
    fat32_fs *fs;
    uint32_t first_cluster;
    uint32_t size;
} fat32_file;

int fat32_mount(fat32_fs *fs, fat32_read_sector_func_t read_sector, void *context);
int fat32_open(fat32_fs *fs, const char *filename, fat32_file *file);
int fat32_read(const fat32_file *file, uint32_t offset, void *buffer, uint32_t length);

#endif
