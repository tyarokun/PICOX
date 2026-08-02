#include "define.h"
#include "lib.h"
#include "fat32.h"
#include "elf_loader.h"
#include "app_memory.h"

#define ET_EXEC 2u      // 実行可能形式のELF
#define EM_ARM  40u     // ARM向けのELF
#define PT_LOAD 1u      // RAMへロードするセグメント
#define MAX_PHNUM 16u   // プログラムヘッダ数の上限
#define MAX_SHNUM 64u   // セクションヘッダ数の上限

#define SHT_REL   9u    // REL形式(再配置可能)
#define SHF_ALLOC 0x2u  // 対象セクションを実行時にRAMへロードするか(rel.debug_infoなどはロードしない)

#define R_ARM_NONE            0u
#define R_ARM_PC24            1u
#define R_ARM_ABS32           2u    // ABS32は32ビットの絶対アドレス
#define R_ARM_REL32           3u
#define R_ARM_THM_CALL       10u
#define R_ARM_THM_PC8        11u
#define R_ARM_CALL           28u
#define R_ARM_JUMP24         29u
#define R_ARM_THM_JUMP24     30u
#define R_ARM_TARGET1        38u
#define R_ARM_V4BX           40u
#define R_ARM_PREL31         42u
#define R_ARM_THM_JUMP19     51u
#define R_ARM_THM_JUMP6      52u
#define R_ARM_GNU_VTINHERIT 100u
#define R_ARM_GNU_VTENTRY   101u
#define R_ARM_THM_JUMP11    102u
#define R_ARM_THM_JUMP8     103u

#define ELF32_R_TYPE(info) ((uint8_t)((info) & 0xffu)) //下位8bitのみ使用 (上位24ビット：シンボル番号, 下位8ビット：再配置型)

// ELFヘッダー
typedef struct __attribute__((packed)){
    uint8_t ident[16];  // ELF識別情報
    uint16_t type;      // ELFの種類
    uint16_t machine;   // 対象CPU
    uint32_t version;
    uint32_t entry;     // 実行開始アドレス
    uint32_t phoff;     // プログラムヘッダ表のファイル内位置
    uint32_t shoff;     // セクションヘッダ表のファイル内位置
    uint32_t flags;
    uint16_t ehsize;    // ELFヘッダのサイズ
    uint16_t phentsize; // プログラムヘッダ1個のサイズ
    uint16_t phnum;     // プログラムヘッダの個数
    uint16_t shentsize;
    uint16_t shnum;     // セクションヘッダの数
    uint16_t shstrndx;
} elf32_header;

// プログラムヘッダー (1つのセグメント)
typedef struct __attribute__((packed)) {
    uint32_t type;      // セグメントの種類
    uint32_t offset;    // ELFファイル内でのデータ位置(セグメント本体の位置)
    uint32_t vaddr;     // ロード先アドレス
    uint32_t paddr;     // 物理アドレス
    uint32_t filesz;    // ELFファイル内に存在するサイズ
    uint32_t memsz;     // RAM上で必要なサイズ
    uint32_t flags;     // 読み書き実行などの属性
    uint32_t align;     // アラインメント
} elf32_program_header;

// セクションヘッダー (1つのセクション)
typedef struct __attribute__((packed)) {
    uint32_t name;
    uint32_t type;      // セクションの種類
    uint32_t flags;     // 属性 (SHF_ALLOCなど)
    uint32_t addr;
    uint32_t offset;
    uint32_t size;
    uint32_t link;
    uint32_t info;      // 再配置対象となるセクション番号
    uint32_t addralign;
    uint32_t entsize;   // 再配置エントリ一個のサイズ
} elf32_section_header;

// REL形式再配置エントリ1個 (ELF仕様で定められた Elf32_Rel という構造体)(再配置1個について, どこを書き換えるか•どの種類の再配置を行うか)
typedef struct __attribute__((packed)) {
    uint32_t offset;    // 再配置によって置き換える場所
    uint32_t info;      // 使用するシンボル番号と再配置方
} elf32_rel;

static int is_power_of_two(uint32_t value){
    return value != 0u && (value & (value - 1u)) == 0u;
}

// ELFファイルの範囲外を読み込まないための検査
static int file_range_valid(const fat32_file *file, uint32_t offset, uint32_t size){
    if(file == NULL || offset > file->size){
        return 0;
    }
    return size <= file->size - offset;
}

// 指定された番号のセクションヘッダを読み込む
static int read_section_header(const fat32_file *file, const elf32_header *header, uint32_t index, elf32_section_header *section){
    uint32_t offset;
    if(file == NULL || header == NULL || section == NULL || index >= header->shnum){
        return -1;
    }
    offset = header->shoff + index * (uint32_t)header->shentsize; // index番目のセクションの位置を計算
    if(!file_range_valid(file, offset, (uint32_t)sizeof(*section))){
        return -1;
    }
    return fat32_read(file, offset, section, (uint32_t)sizeof(*section)); //index番目のセクションから読み込む
}

// ロード済みRAMから、32ビット値をリトルエンディアンとして読む
static uint32_t read_u32_le(uint32_t address){
    const uint8_t *p = (const uint8_t *)address; //1バイト (Cortex-M0+ではアラインメントが合っていない32ビットアクセスが問題になる可能性があるため1バイトずつ)
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

// 32ビット値をリトルエンディアン形式でRAMへ書き戻す
static void write_u32_le(uint32_t address, uint32_t value){
    uint8_t *p = (uint8_t *)address;
    p[0] = (uint8_t)value;
    p[1] = (uint8_t)(value >> 8);
    p[2] = (uint8_t)(value >> 16);
    p[3] = (uint8_t)(value >> 24);
}

/*
 * ELFをlinked_baseからload_baseへ丸ごと移動したとき、
 * PC相対値は変化しないため、内部を指す絶対アドレスだけを差分補正する。
 * アプリELFは最終リンク時に--emit-relocsを指定し、.rel.*を残しておく。
 */
static int apply_relocations(const fat32_file *file, const elf32_header *header, const elf_loaded_image *image){
    elf32_section_header relocation_section;
    elf32_section_header target_section;
    elf32_rel relocation;
    uint32_t section_table_size;
    uint32_t relocation_count;
    uint32_t relocation_offset;
    uint32_t linked_end;
    uint32_t load_delta;
    uint32_t target_address;
    uint32_t value;
    uint32_t i;
    uint32_t j;
    uint8_t type;

    if(file == NULL || header == NULL || image == NULL){
        return ELF_LOADER_ERR_FORMAT;
    }
    load_delta = image->load_base - image->linked_base; // 元のロード位置と再配置先の差を計算
    if(load_delta == 0u || header->shnum == 0u){ // 再配置が必要ない時はreturn
        return ELF_LOADER_OK;
    }
    if(header->shoff == 0u || header->shentsize != sizeof(elf32_section_header) || header->shnum > MAX_SHNUM){
        return ELF_LOADER_ERR_FORMAT;
    }
    section_table_size = (uint32_t)header->shnum * (uint32_t)header->shentsize; // セクションヘッダーテーブルのサイズを計算
    if(!file_range_valid(file, header->shoff, section_table_size)){
        return ELF_LOADER_ERR_FORMAT;
    }

    linked_end = image->linked_base + image->image_size; // リンク時イメージの末尾

    // 全セクションを順番に調べる
    for(i = 0u; i < header->shnum; i++){
        // i番目のセクション読み取り
        if(read_section_header(file, header, i, &relocation_section) < 0){
            return ELF_LOADER_ERR_READ;
        }
        // SHT_RELか、サイズが0ではないか
        if(relocation_section.type != SHT_REL || relocation_section.size == 0u){
            continue;
        }
        if(relocation_section.info >= header->shnum){
            return ELF_LOADER_ERR_FORMAT;
        }
        // 該当するセクション番号(info)のセクションをtarget_sectionへ格納
        if(read_section_header(file, header, relocation_section.info, &target_section) < 0){
            return ELF_LOADER_ERR_READ;
        }
        // --emit-relocsではデバッグセクション用の再配置も残るため、RAMへ配置されるセクションだけ処理する
        if((target_section.flags & SHF_ALLOC) == 0u){
            continue;
        }
        if(relocation_section.entsize != sizeof(elf32_rel) || (relocation_section.size % sizeof(elf32_rel)) != 0u || !file_range_valid(file, relocation_section.offset, relocation_section.size)){
            return ELF_LOADER_ERR_FORMAT;
        }
        
        // 再配置エントリを順番に読む
        relocation_count = relocation_section.size / (uint32_t)sizeof(elf32_rel); // 再配置エントリの個数
        for(j = 0u; j < relocation_count; j++){
            relocation_offset = relocation_section.offset + j * (uint32_t)sizeof(elf32_rel); // 該当する Relocation section のファイル内位置を計算
            if(fat32_read(file, relocation_offset, &relocation, sizeof(relocation)) < 0){ // 該当する Relocation section を読み込む
                return ELF_LOADER_ERR_READ;
            }
            type = ELF32_R_TYPE(relocation.info); // 再配置型を取得
            if(type == R_ARM_NONE){
                continue;
            }
            if(relocation.offset < image->linked_base || relocation.offset >= linked_end){
                return ELF_LOADER_ERR_RANGE;
            }
            target_address = image->load_base + (relocation.offset - image->linked_base); // ロード後の修正先アドレスを計算 (relocation.offset-linked_baseは相対距離なのでそれをload_baseへ足せば機械語上での変数の位置がわかる)

            switch(type){
                case R_ARM_ABS32:
                case R_ARM_TARGET1:
                    // 修正場所から4バイト読めるか確認
                    if(linked_end - relocation.offset < sizeof(uint32_t)){
                        return ELF_LOADER_ERR_RANGE;
                    }
                    // 現在保存されている値(32bit)を読む
                    value = read_u32_le(target_address);
                    // 内部を指す値だけを移動し、MMIOなどイメージ外の固定アドレスは変更しない
                    if(value >= image->linked_base && value <= linked_end){
                        write_u32_le(target_address, value + load_delta); // 値がリンク時のアプリイメージ内部を指している場合だけ差分を加える
                    }
                    break;
                // イメージ全体を同じ差分だけ移動する場合、S-Pは変化しない
                case R_ARM_PC24:
                case R_ARM_REL32:
                case R_ARM_THM_CALL:
                case R_ARM_THM_PC8:
                case R_ARM_CALL:
                case R_ARM_JUMP24:
                case R_ARM_THM_JUMP24:
                case R_ARM_PREL31:
                case R_ARM_THM_JUMP19:
                case R_ARM_THM_JUMP6:
                case R_ARM_THM_JUMP11:
                case R_ARM_THM_JUMP8:
                case R_ARM_V4BX:
                case R_ARM_GNU_VTINHERIT:
                case R_ARM_GNU_VTENTRY:
                    break;
                default:
                    // Cortex-M0+向けの最小実装で未対応の再配置は拒否する
                    return ELF_LOADER_ERR_FORMAT;
            }
        }
    }
    return ELF_LOADER_OK;
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
        program_offset = header.phoff + i * (uint32_t)header.phentsize; // i番目のプログラムヘッダのファイル内オフセットを計算
        if (fat32_read(file, program_offset, &program, sizeof(program)) < 0){// ELFファイルのプログラムヘッダーを読み込む
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
        segment_end = program.vaddr + program.memsz;    // 全てのPT_LOADの中で最も大きい末尾アドレス
        if(program.vaddr < image_start){    // すべてのPT_LOADの開始アドレスを比較し、最も小さいものを選ぶ
            image_start = program.vaddr;    // 全てのPT_LOADの中で最も小さいvaddr
        }
        if(segment_end > image_end){    // すべてのPT_LOADの末尾アドレスを比較し、最も大きいものを選ぶ
            image_end = segment_end;    // 全てのPT_LOADの中で最も大きい末尾アドレス
        }
        if(program.align > max_alignment){
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
    if (linked_entry < image_start || linked_entry >= image_end) {
        return ELF_LOADER_ERR_RANGE;
    }

    // 空き領域を確保する
    if (app_memory_alloc(image_size, max_alignment, &image->memory) < 0){
        return ELF_LOADER_ERR_NO_MEMORY;
    }

    // 確保した領域だけを初期化する
    memset((void *)image->memory.base, 0, (long)image->memory.size);

    // 2回目の走査 (各PT_LOADを、元の相対位置を保ったまま確保した領域へコピーする)
    for (i = 0u; i < header.phnum; i++){
        program_offset = header.phoff + i * (uint32_t)header.phentsize;
        if(fat32_read(file, program_offset, &program, sizeof(program)) < 0){
            return load_error(image, ELF_LOADER_ERR_READ);
        }
        if(program.type != PT_LOAD || program.memsz == 0u){
            continue;
        }

        // 実ロード先 :load_base + (ELF内のvaddr - ELFの先頭vaddr)
        destination = image->memory.base + (program.vaddr - image_start);

        if(program.filesz != 0u){
            if (fat32_read(file, program.offset, (void *)destination, program.filesz) < 0){ //セグメント本体をdestinationへロード
                return load_error(image, ELF_LOADER_ERR_READ);
            }
        }
        // .bss相当部分をゼロ初期化する
        if(program.memsz > program.filesz){
            memset((void *)(destination + program.filesz), 0, (long)(program.memsz - program.filesz));
        }
    }

    image->linked_base = image_start;
    image->load_base = image->memory.base;
    image->image_size = image_size;
    image->entry = image->load_base + (linked_entry - image_start); // entryも新しいロード先へ移動する
    image->entry |= 1u; // Thumbビットを付ける

    
    int result = apply_relocations(file, &header, image);
    if(result < 0){ // 再配置処理
        return load_error(image, result);
    }

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
