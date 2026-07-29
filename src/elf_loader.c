#include "define.h"
#include "lib.h"
#include "fat32.h"
#include "elf_loader.h"

#define APP_RAM_START 0x20020000u
#define APP_RAM_END   0x20040000u

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

int elf_loader_load(const fat32_file *file, uint32_t *entry){ // file：FAT32上で開かれたELFファイル, entry：ロード後に実行開始アドレスを書き込む変数
    elf32_header header;
    elf32_program_header program;
    uint32_t i;
    uint32_t loaded = 0u;

    if(file == NULL || entry == NULL){
        return ELF_LOADER_ERR_HEADER;
    }

    // ファイル先頭のオフセット0から、ELFヘッダのサイズだけ読み込みます。
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

    // ELF形式の確認
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

    // アプリケーション用RAM領域全体をゼロで初期化
    memset((void *)APP_RAM_START, 0, (long)(APP_RAM_END - APP_RAM_START));

    //ELFに含まれるすべてのプログラムヘッダのブロックを順番に調べる
    for(i = 0u; i < header.phnum; i++){
        uint32_t program_offset = header.phoff + i * (uint32_t)header.phentsize; // プログラムヘッダ表の先頭 + ヘッダ番号 × ヘッダ1個のサイズ

        // 計算した位置からプログラムヘッダを1個読み込む
        if(fat32_read(file, program_offset, &program, sizeof(program)) < 0){
            return ELF_LOADER_ERR_READ;
        }

        // ロード対象の確認 (PT_LOADでないセグメントはRAMへロードする必要がないため無視 || RAM上で必要なサイズが0なら何も配置する必要がないため無視)
        if(program.type != PT_LOAD || program.memsz == 0u){
            continue;
        }

        // セグメントの範囲検査 (不正なサイズや、PICOX本体の領域を壊すような配置を防ぐ)
        if(program.filesz > program.memsz
            || program.vaddr < APP_RAM_START
            || program.vaddr >= APP_RAM_END
            || program.memsz > APP_RAM_END - program.vaddr){
            return ELF_LOADER_ERR_RANGE;
        }

        // ELFファイル内のprogram.offsetからprogram.fileszバイト読み込み、RAMのprogram.vaddrへ直接配置
        if(program.filesz != 0u && fat32_read(file, program.offset, (void *)program.vaddr, program.filesz) < 0){
            return ELF_LOADER_ERR_READ;
        }

        // .bss相当部分のゼロ初期化
        if(program.memsz > program.filesz){
            memset((void *)(program.vaddr + program.filesz), 0, (long)(program.memsz - program.filesz));
        }

        loaded++; //正常にロードしたPT_LOADセグメントの数を記録
    }

    // 最後まで処理しても1つもロードされなかった場合はエラー
    if(loaded == 0u){
        return ELF_LOADER_ERR_NO_SEGMENT;
    }

    __asm__ volatile ("dsb" ::: "memory");  // それ以前に行ったメモリ書き込みが完了するまで、後続処理を進めないようする (ELFのコードやデータがRAMへ確実に書き込まれるのを待つ)
    __asm__ volatile ("isb" ::: "memory");  // CPUの命令実行パイプラインを同期し、新しく書き込まれた命令を正しく参照できるようにする

    *entry = header.entry;  // エントリアドレスを返す
    return ELF_LOADER_OK;
}
