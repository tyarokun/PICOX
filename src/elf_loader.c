#include "define.h"
#include "lib.h"
#include "fat32.h"
#include "elf_loader.h"

#define APP_RAM_START 0x20020000u
#define APP_RAM_END   0x20040000u

#define ET_EXEC 2u
#define EM_ARM  40u
#define PT_LOAD 1u
#define MAX_PHNUM 16u

typedef struct __attribute__((packed)) {
    uint8_t ident[16];
    uint16_t type;
    uint16_t machine;
    uint32_t version;
    uint32_t entry;
    uint32_t phoff;
    uint32_t shoff;
    uint32_t flags;
    uint16_t ehsize;
    uint16_t phentsize;
    uint16_t phnum;
    uint16_t shentsize;
    uint16_t shnum;
    uint16_t shstrndx;
} elf32_header;

typedef struct __attribute__((packed)) {
    uint32_t type;
    uint32_t offset;
    uint32_t vaddr;
    uint32_t paddr;
    uint32_t filesz;
    uint32_t memsz;
    uint32_t flags;
    uint32_t align;
} elf32_program_header;

int elf_loader_load(const fat32_file *file, uint32_t *entry){
    elf32_header header;
    elf32_program_header program;
    uint32_t i;
    uint32_t loaded = 0u;

    if(file == NULL || entry == NULL){
        return ELF_LOADER_ERR_HEADER;
    }

    if(fat32_read(file, 0u, &header, sizeof(header)) < 0){
        return ELF_LOADER_ERR_HEADER;
    }

    if(header.ident[0] != 0x7fu
        || header.ident[1] != 'E'
        || header.ident[2] != 'L'
        || header.ident[3] != 'F'){
        return ELF_LOADER_ERR_HEADER;
    }

    if(header.ident[4] != 1u
        || header.ident[5] != 1u
        || header.type != ET_EXEC
        || header.machine != EM_ARM
        || header.ehsize != sizeof(header)
        || header.phentsize != sizeof(program)
        || header.phnum == 0u
        || header.phnum > MAX_PHNUM){
        return ELF_LOADER_ERR_FORMAT;
    }

    if((header.entry & 1u) == 0u || header.entry < APP_RAM_START || header.entry >= APP_RAM_END){
        return ELF_LOADER_ERR_RANGE;
    }

    memset((void *)APP_RAM_START, 0, (long)(APP_RAM_END - APP_RAM_START));

    for(i = 0u; i < header.phnum; i++){
        uint32_t program_offset = header.phoff + i * (uint32_t)header.phentsize;

        if(fat32_read(file, program_offset, &program, sizeof(program)) < 0){
            return ELF_LOADER_ERR_READ;
        }

        if(program.type != PT_LOAD || program.memsz == 0u){
            continue;
        }

        if(program.filesz > program.memsz
            || program.vaddr < APP_RAM_START
            || program.vaddr >= APP_RAM_END
            || program.memsz > APP_RAM_END - program.vaddr){
            return ELF_LOADER_ERR_RANGE;
        }

        if(program.filesz != 0u && fat32_read(file, program.offset, (void *)program.vaddr, program.filesz) < 0){
            return ELF_LOADER_ERR_READ;
        }

        if(program.memsz > program.filesz){
            memset((void *)(program.vaddr + program.filesz), 0, (long)(program.memsz - program.filesz));
        }

        loaded++;
    }

    if(loaded == 0u){
        return ELF_LOADER_ERR_NO_SEGMENT;
    }

    __asm__ volatile ("dsb" ::: "memory");
    __asm__ volatile ("isb" ::: "memory");

    *entry = header.entry;
    return ELF_LOADER_OK;
}
