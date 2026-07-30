#ifndef ELF_LOADER_H
#define ELF_LOADER_H

#include "define.h"
#include "fat32.h"
#include "app_memory.h"

typedef enum {
    ELF_LOADER_OK = 0,
    ELF_LOADER_ERR_HEADER = -1,
    ELF_LOADER_ERR_FORMAT = -2,
    ELF_LOADER_ERR_RANGE = -3,
    ELF_LOADER_ERR_READ = -4,
    ELF_LOADER_ERR_NO_SEGMENT = -5,
    ELF_LOADER_ERR_NO_MEMORY = -6
} elf_loader_result;

typedef struct {
    uint32_t entry;             // 実際のロード先に補正したエントリアドレス
    uint32_t linked_base;       // ELFがリンクされた元の先頭アドレス
    uint32_t load_base;         // 実際にロードした先頭アドレス
    uint32_t image_size;        // PT_LOADセグメント全体が占める範囲
    app_memory_region memory;   // app_memoryで確保した領域
} elf_loaded_image;

/* FAT32上のELFを解析し、PT_LOADセグメントをアプリRAMへ配置する。 */
int elf_loader_load(const fat32_file *file, elf_loaded_image *image);
void elf_loader_unload(elf_loaded_image *image);

#endif
