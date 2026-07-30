#include "define.h"
#include "lib.h"
#include "fat32.h"
#include "elf_loader.h"
#include "app_memory.h"

#define ET_EXEC 2u      // 実行可能形式のELF
#define EM_ARM  40u     // ARM向けのELF
#define PT_LOAD 1u      // RAMへロードするセグメント
#define MAX_PHNUM 16u   // プログラムヘッダ数の上限

// ELFヘッダー
typedef struct __attribute__((packed)){
    uint8_t ident[16];  // ELF識別情報
    uint16_t type;      // ELFの種類
    uint16_t machine;   // 対象CPU
    uint32_t version;   // 実行開始アドレス
    uint32_t entry;     // 実行開始アドレス
    uint32_t phoff;     // プログラムヘッダ表のファイル内位置 (プログラムヘッダ表の先頭)
    uint32_t shoff;
    uint32_t flags;
    uint16_t ehsize;    // ELFヘッダのサイズ
    uint16_t phentsize; // プログラムヘッダ1個のサイズ
    uint16_t phnum;     // プログラムヘッダの個数
    uint16_t shentsize;
    uint16_t shnum;
    uint16_t shstrndx;
} elf32_header;

// プログラムヘッダー
typedef struct __attribute__((packed)) {
    uint32_t type;      // セグメントの種類
    uint32_t offset;    // ELFファイル内でのデータ位置
    uint32_t vaddr;     // ロード先アドレス
    uint32_t paddr;     // 物理アドレス
    uint32_t filesz;    // ELFファイル内に存在するサイズ
    uint32_t memsz;     // RAM上で必要なサイズ
    uint32_t flags;     // 読み書き実行などの属性
    uint32_t align;     // アラインメント
} elf32_program_header;

static int is_power_of_two(uint32_t value){
    return value != 0u && (value & (value - 1u)) == 0u;
}

static int load_error(elf_loaded_image *image, int error){
    if (image != NULL && image->memory.page_count != 0u){
        app_memory_free(&image->memory);
    }
    if (image != NULL) {
        memset(image, 0, (long)sizeof(*image));
    }
    return error;
}


int elf_loader_load(const fat32_file *file, elf_loaded_image *image){ // file：FAT32上で開かれたELFファイル, entry：ロード後に実行開始アドレスを書き込む変数
    elf32_header header;
    elf32_program_header program;
    uint32_t i;
    uint32_t image_start = 0xffffffffu;
    uint32_t image_end = 0u;
    uint32_t image_size;
    uint32_t max_alignment = 4u;
    uint32_t linked_entry;
    uint32_t destination;
    uint32_t segment_end;
    uint32_t program_offset;
    uint32_t segment_count = 0u;

    if(file == NULL || image == NULL){
        return ELF_LOADER_ERR_HEADER;
    }

    memset(image, 0, (long)sizeof(*image));

    // ファイル先頭のオフセット0から、ELFヘッダのサイズだけ読み込む
    if(fat32_read(file, 0u, &header, sizeof(header)) < 0){
        return ELF_LOADER_ERR_HEADER;
    }

    // マジックナンバーの確認
    if(header.ident[0] != 0x7fu
        || header.ident[1] != 'E'
        || header.ident[2] != 'L'
        || header.ident[3] != 'F'){
        return ELF_LOADER_ERR_HEADER;
    }

    // 対応しているELF形式か確認
    if(header.ident[4] != 1u                    // 32ビットRLF
        || header.ident[5] != 1u                // Cortex-M0+ はリトルエンディアン
        || header.type != ET_EXEC               // 実行可能ファイルか確認 (固定配置されたリンクされた実行ファイル)(共有オブジェクトや再配置可能オブジェクトはロードできない)
        || header.machine != EM_ARM             // ARM向けのELF
        || header.ehsize != sizeof(header)      // ELFヘッダのサイズが、このプログラムで定義したelf32_headerのサイズと一致するか確認
        || header.phentsize != sizeof(program)  // プログラムヘッダ1個のサイズが、elf32_program_headerのサイズと一致するか確認
        || header.phnum == 0u                   // header.phnum == 0u なら、プログラムヘッダが存在しないためロードできない
        || header.phnum > MAX_PHNUM){           // header.phnum > MAX_PHNUM なら、異常なELFや想定外に複雑なELFとして拒否
        return ELF_LOADER_ERR_FORMAT;
    }

    // 実行開始アドレスがThumb関数として正しく設定されているか (Cortex-Mでは、関数ポインタの最下位ビットを1にして、Thumb命令であることを示す)
    if((header.entry & 1u) == 0u || header.entry < APP_RAM_START || header.entry >= APP_RAM_END){
        return ELF_LOADER_ERR_RANGE;
    }

    // 1回目の走査 (PT_LOAD全体の先頭・末尾・最大アラインメントを求める)(この時点ではまだRAMへコピーしない)
    for(i = 0u; i < header.phnum; i++){
        program_offset = header.phoff + i * (uint32_t)header.phentsize;
        if (fat32_read(file, program_offset, &program, sizeof(program)) < 0){
            return ELF_LOADER_ERR_READ;
        }
        if (program.type != PT_LOAD || program.memsz == 0u) {
            continue;
        }
        // 元のELFがPICOXのアプリ用アドレスへ リンクされていることを確認
        if (program.filesz > program.memsz || program.vaddr < APP_RAM_START || program.vaddr >= APP_RAM_END || program.memsz > APP_RAM_END - program.vaddr) {
            return ELF_LOADER_ERR_RANGE;
        }
        // p_alignは0、1または2の累乗でなければならない
        if (program.align > 1u && !is_power_of_two(program.align)) {
            return ELF_LOADER_ERR_FORMAT;
        }
        segment_end = program.vaddr + program.memsz;
        if (program.vaddr < image_start) {
            image_start = program.vaddr;
        }
        if (segment_end > image_end) {
            image_end = segment_end;
        }
        if (program.align > max_alignment) {
            max_alignment = program.align;
        }
        segment_count++;
    }

    if (segment_count == 0u) {
        return ELF_LOADER_ERR_NO_SEGMENT;
    }
    if (image_end <= image_start) {
        return ELF_LOADER_ERR_RANGE;
    }
    image_size = image_end - image_start;

    // Thumbビットを除いた元のentryがPT_LOAD範囲内にあることを確認する
    linked_entry = header.entry & ~1u;
    if (linked_entry < image_start
        || linked_entry >= image_end) {
        return ELF_LOADER_ERR_RANGE;
    }

    // 空き領域を確保する
    if (app_memory_alloc(image_size, max_alignment, &image->memory) < 0){
        return ELF_LOADER_ERR_NO_MEMORY;
    }

    // 確保した領域だけを初期化する
    memset((void *)image->memory.base, 0, (long)image->memory.size);

    /*
     * 2回目の走査
     *
     * 各PT_LOADを、元の相対位置を保ったまま
     * 確保した領域へコピーする。
     */
    for (i = 0u; i < header.phnum; i++){
        program_offset = header.phoff + i * (uint32_t)header.phentsize;
        if(fat32_read(file, program_offset, &program, sizeof(program)) < 0){
            return load_error(image, ELF_LOADER_ERR_READ);
        }
        if(program.type != PT_LOAD || program.memsz == 0u){
            continue;
        }

        // 実ロード先 load_base + (ELF内のvaddr - ELFの先頭vaddr)
        destination = image->memory.base + (program.vaddr - image_start);

        if (program.filesz != 0u) {
            if (fat32_read(file, program.offset, (void *)destination, program.filesz) < 0){
                return load_error(image, ELF_LOADER_ERR_READ);
            }
        }
        // .bss相当部分をゼロ初期化する
        if (program.memsz > program.filesz) {
            memset((void *)(destination + program.filesz), 0, (long)(program.memsz - program.filesz));
        }
    }

    image->linked_base = image_start;
    image->load_base = image->memory.base;
    image->image_size = image_size;

    // entryも新しいロード先へ移動する
    image->entry = image->load_base + (linked_entry - image_start);

    // Thumbビットを付ける
    image->entry |= 1u;

    __asm__ volatile ("dsb" ::: "memory");
    __asm__ volatile ("isb" ::: "memory");

    return ELF_LOADER_OK;
}

void elf_loader_unload(elf_loaded_image *image){
    if (image == NULL) {
        return;
    }
    app_memory_free(&image->memory);
    memset(image, 0, (long)sizeof(*image));
}
