#ifndef SD_ELF_H
#define SD_ELF_H

#include "define.h"

/* FAT32 root directoryから APP.ELF をRAMへ読み込む。 */
int sd_elf_load(char *filename, uint32_t *entry);
const char *sd_elf_error(int error);

#endif
