#ifndef ELF_LOADER_H
#define ELF_LOADER_H

#include "define.h"
#include "fat32.h"

typedef enum {
    ELF_LOADER_OK = 0,
    ELF_LOADER_ERR_HEADER = -1,
    ELF_LOADER_ERR_FORMAT = -2,
    ELF_LOADER_ERR_RANGE = -3,
    ELF_LOADER_ERR_READ = -4,
    ELF_LOADER_ERR_NO_SEGMENT = -5
} elf_loader_result;

/* FAT32上のELFを解析し、PT_LOADセグメントをアプリRAMへ配置する。 */
int elf_loader_load(const fat32_file *file, uint32_t *entry);

#endif
