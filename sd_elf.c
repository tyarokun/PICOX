#include "define.h"
#include "lib.h"
#include "sd_elf.h"

#define REG32(address) (*(volatile uint32_t *)(address))

#define APP_RAM_START 0x20020000u
#define APP_RAM_END   0x20040000u
#define SECTOR_SIZE   512u

/* RP2040 registers */
#define RESETS_BASE     0x4000C000u
#define IO_BANK0_BASE   0x40014000u
#define PADS_BANK0_BASE 0x4001C000u
#define SPI0_BASE       0x4003C000u
#define SIO_BASE        0xD0000000u

#define RESETS_RESET      REG32(RESETS_BASE + 0x00u)
#define RESETS_RESET_DONE REG32(RESETS_BASE + 0x08u)
#define RESET_SPI0        (1u << 16)

#define GPIO_CTRL(gpio) REG32(IO_BANK0_BASE + 0x04u + (gpio) * 8u)
#define GPIO_PAD(gpio)  REG32(PADS_BANK0_BASE + 0x04u + (gpio) * 4u)
#define GPIO_FUNC_SPI   1u
#define GPIO_FUNC_SIO   5u
#define PAD_IE           (1u << 6)
#define PAD_PUE          (1u << 3)
#define PAD_PDE          (1u << 2)

#define SIO_GPIO_OUT_SET REG32(SIO_BASE + 0x14u)
#define SIO_GPIO_OUT_CLR REG32(SIO_BASE + 0x18u)
#define SIO_GPIO_OE_SET  REG32(SIO_BASE + 0x24u)

#define SSPCR0  REG32(SPI0_BASE + 0x00u)
#define SSPCR1  REG32(SPI0_BASE + 0x04u)
#define SSPDR   REG32(SPI0_BASE + 0x08u)
#define SSPSR   REG32(SPI0_BASE + 0x0cu)
#define SSPCPSR REG32(SPI0_BASE + 0x10u)
#define SSPCR1_SSE (1u << 1)
#define SSPSR_TNF  (1u << 1)
#define SSPSR_RNE  (1u << 2)
#define SSPSR_BSY  (1u << 4)

#define GPIO_MISO 16u
#define GPIO_CS   17u
#define GPIO_SCK  18u
#define GPIO_MOSI 19u
#define CS_MASK   (1u << GPIO_CS)

/* SD commands */
#define CMD0   0u
#define CMD8   8u
#define CMD16 16u
#define CMD17 17u
#define CMD55 55u
#define CMD58 58u
#define ACMD41 41u

/* ELF */
#define ET_EXEC 2u
#define EM_ARM  40u
#define PT_LOAD 1u

#define ERR_SD_INIT     -1
#define ERR_SD_READ     -2
#define ERR_FAT32       -3
#define ERR_NOT_FOUND   -4
#define ERR_ELF_HEADER  -5
#define ERR_ELF_FORMAT  -6
#define ERR_ELF_RANGE   -7
#define ERR_ELF_READ    -8
#define ERR_NO_SEGMENT  -9
#define ERR_FILENAME   -10

#define MAX_PHNUM 16u

static uint8_t sector_buffer[SECTOR_SIZE];
static int sd_block_addressing;

static uint32_t fat_start_lba;
static uint32_t data_start_lba;
static uint32_t root_cluster;
static uint32_t sectors_per_cluster;

typedef struct {
    uint32_t first_cluster;
    uint32_t size;
} fat_file;

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

static uint16_t get_u16(const uint8_t *p){
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

static uint32_t get_u32(const uint8_t *p){
    return (uint32_t)p[0]
        | ((uint32_t)p[1] << 8)
        | ((uint32_t)p[2] << 16)
        | ((uint32_t)p[3] << 24);
}

/* ---------- SPI ---------- */

static uint8_t spi_transfer(uint8_t value){
    while((SSPSR & SSPSR_TNF) == 0u);//送信FIFOに空きが出るまで待つ
    SSPDR = value; //1バイト書き込む
    while((SSPSR & SSPSR_RNE) == 0u);//受信FIFOにデータが届くまで待つ
    return (uint8_t)SSPDR; //受信した1バイトを返す
}

//SDカードを選択
static void cs_high(void){
    SIO_GPIO_OUT_SET = CS_MASK;
}

//SDカードの選択を解除
static void cs_low(void){
    SIO_GPIO_OUT_CLR = CS_MASK;
}

static void spi_set_speed(uint32_t divider){
    uint32_t dummy;

    SSPCR1 = 0u;
    while ((SSPSR & SSPSR_RNE) != 0u) {
        dummy = SSPDR;
        (void)dummy;
    }
    SSPCR0 = (1u << 8) | 7u; /* mode 0, 8 bit, SCR=1 */
    SSPCPSR = divider;
    SSPCR1 = SSPCR1_SSE;
}

static void spi_init(void){
    RESETS_RESET &= ~RESET_SPI0;
    while ((RESETS_RESET_DONE & RESET_SPI0) == 0u) {
    }

    GPIO_CTRL(GPIO_MISO) = GPIO_FUNC_SPI;
    GPIO_CTRL(GPIO_SCK) = GPIO_FUNC_SPI;
    GPIO_CTRL(GPIO_MOSI) = GPIO_FUNC_SPI;
    GPIO_CTRL(GPIO_CS) = GPIO_FUNC_SIO;

    GPIO_PAD(GPIO_MISO) = (GPIO_PAD(GPIO_MISO) | PAD_IE | PAD_PUE) & ~PAD_PDE;
    GPIO_PAD(GPIO_CS) = (GPIO_PAD(GPIO_CS) | PAD_PUE) & ~PAD_PDE;

    cs_high();
    SIO_GPIO_OE_SET = CS_MASK;
    spi_set_speed(250u); /* 約250 kHz */
}

/* ---------- SD card ---------- */

static void sd_deselect(void){
    cs_high();
    (void)spi_transfer(0xffu);
}

// 6バイトを送信 (コマンド番号:1バイト, 引数:バイト, CRC:1バイト)
static uint8_t sd_command(uint8_t command, uint32_t argument){
    uint8_t response = 0xffu;
    uint8_t crc = 0x01u;
    uint32_t i;

    sd_deselect();
    cs_low();
    (void)spi_transfer(0xffu);

    if (command == CMD0) {
        crc = 0x95u;
    } else if (command == CMD8) {
        crc = 0x87u;
    }

    (void)spi_transfer((uint8_t)(0x40u | command));
    (void)spi_transfer((uint8_t)(argument >> 24));
    (void)spi_transfer((uint8_t)(argument >> 16));
    (void)spi_transfer((uint8_t)(argument >> 8));
    (void)spi_transfer((uint8_t)argument);
    (void)spi_transfer(crc);

    for (i = 0u; i < 16u; i++) {
        response = spi_transfer(0xffu);
        if ((response & 0x80u) == 0u) {
            break;
        }
    }
    return response;
}

static int sd_init(void){
    uint8_t response;
    uint8_t r7[4];
    uint8_t ocr[4];
    uint32_t i;
    int version2 = 0;

    sd_block_addressing = 0;
    spi_init();

    for (i = 0u; i < 10u; i++) {
        (void)spi_transfer(0xffu);
    }

    //SDカードをアイドル状態へ移行 (成功時はR1 = 0x01が返る)
    response = 0xffu;
    for (i = 0u; i < 10u; i++) {
        response = sd_command(CMD0, 0u);
        sd_deselect();
        if (response == 0x01u) {
            break;
        }
    }
    if (response != 0x01u) {
        return -1;
    }

    //SDカードがVersion 2以降か確認 (0x01AAであれば対応電圧や通信が正しいと判断)
    response = sd_command(CMD8, 0x1aau);
    if (response == 0x01u) {
        for (i = 0u; i < 4u; i++) {
            r7[i] = spi_transfer(0xffu);
        }
        sd_deselect();
        if (r7[2] != 0x01u || r7[3] != 0xaau) {
            return -1;
        }
        version2 = 1;
    } else if ((response & 0x04u) != 0u) {
        sd_deselect();
    } else {
        sd_deselect();
        return -1;
    }

    // CMD55, ACMD41 を繰り返し、SDカードの初期化完了を待つ
    response = 0xffu;
    for (i = 0u; i < 5000u; i++) {
        response = sd_command(CMD55, 0u);
        sd_deselect();
        if (response > 0x01u) {
            continue;
        }
        response = sd_command(ACMD41, version2 ? 0x40000000u : 0u);
        sd_deselect();
        if (response == 0u) {
            break;
        }
    }
    if (response != 0u) {
        return -1;
    }

    // CMD58でOCRレジスタを読む
    response = sd_command(CMD58, 0u);
    if (response != 0u) {
        sd_deselect();
        return -1;
    }
    for (i = 0u; i < 4u; i++) {
        ocr[i] = spi_transfer(0xffu);
    }
    sd_deselect();

    if (version2 && (ocr[0] & 0x40u) != 0u) {
        sd_block_addressing = 1;
    } else {
        response = sd_command(CMD16, SECTOR_SIZE);
        sd_deselect();
        if (response != 0u) {
            return -1;
        }
    }

    while ((SSPSR & SSPSR_BSY) != 0u) {
    }
    spi_set_speed(4u); /* 約15.6 MHz */
    return 0;
}

// 指定した512バイトセクタを読み取る (CMD17を送信→SDカードの応答を確認→0xFEデータトークンを待つ→512バイト受信→CRC 2バイトを読み捨てる)
static int sd_read_sector(uint32_t lba, uint8_t *buffer){
    uint32_t argument = sd_block_addressing ? lba : lba * SECTOR_SIZE;
    uint8_t response;
    uint8_t token = 0xffu;
    uint32_t i;

    response = sd_command(CMD17, argument);
    if (response != 0u) {
        sd_deselect();
        return -1;
    }

    for (i = 0u; i < 200000u; i++) {
        token = spi_transfer(0xffu);
        if (token != 0xffu) {
            break;
        }
    }
    if (token != 0xfeu) {
        sd_deselect();
        return -1;
    }

    for (i = 0u; i < SECTOR_SIZE; i++) {
        buffer[i] = spi_transfer(0xffu);
    }
    (void)spi_transfer(0xffu);
    (void)spi_transfer(0xffu);
    sd_deselect();
    return 0;
}

/* ---------- FAT32 ---------- */
static int looks_like_fat32(const uint8_t *boot){
    return get_u16(boot + 11u) == SECTOR_SIZE && boot[13] != 0u && get_u16(boot + 17u) == 0u && get_u16(boot + 22u) == 0u && get_u32(boot + 36u) != 0u && boot[510] == 0x55u && boot[511] == 0xaau;
}

// SDカードのFAT32情報を解析
static int fat_mount(void){
    uint32_t boot_lba = 0u;
    uint32_t reserved;
    uint32_t fat_size;
    uint32_t fat_count;

    if(sd_read_sector(0u, sector_buffer) < 0){ //最初にセクタ0を読み、セクタ0自体がFAT32ブートセクタなら、そのまま使用
        return -1;
    }

    if(!looks_like_fat32(sector_buffer)){
        boot_lba = get_u32(sector_buffer + 446u + 8u); //そうでなければMBRと判断し、最初のパーティションの開始LBAを読む
        if (boot_lba == 0u || sd_read_sector(boot_lba, sector_buffer) < 0 || !looks_like_fat32(sector_buffer)) {
            return -1;
        }
    }

    // FAT32のブートセクタから次を取得
    sectors_per_cluster = sector_buffer[13];
    reserved = get_u16(sector_buffer + 14u);
    fat_count = sector_buffer[16];
    fat_size = get_u32(sector_buffer + 36u);
    root_cluster = get_u32(sector_buffer + 44u) & 0x0fffffffu;

    if (reserved == 0u || fat_count == 0u || root_cluster < 2u || (sectors_per_cluster & (sectors_per_cluster - 1u)) != 0u) {
        return -1;
    }

    fat_start_lba = boot_lba + reserved;
    data_start_lba = fat_start_lba + fat_count * fat_size;
    return 0;
}

// クラスタ番号からセクタ番号への変換
static uint32_t cluster_lba(uint32_t cluster){
    return data_start_lba + (cluster - 2u) * sectors_per_cluster;
}

// 次のクラスタを取得
static int fat_next_cluster(uint32_t cluster, uint32_t *next){
    uint32_t offset = cluster * 4u;

    if (sd_read_sector(fat_start_lba + offset / SECTOR_SIZE,
                       sector_buffer) < 0) {
        return -1;
    }
    *next = get_u32(sector_buffer + offset % SECTOR_SIZE) & 0x0fffffffu;
    return 0;
}

//小文字ファイル名は大文字へ変換
static uint8_t fat_upper(uint8_t c){
    if (c >= 'a' && c <= 'z') {
        return (uint8_t)(c - ('a' - 'A'));
    }
    return c;
}

// ファイル名変 (FATディレクトリエントリの短いファイル名は11バイト固定)
static int fat_make_short_name(const char *filename, uint8_t name[11]){
    uint32_t base_length = 0u;
    uint32_t extension_length = 0u;
    int extension = 0;
    uint8_t c;
    uint32_t i;

    if (filename == NULL || *filename == '\0') {
        return -1;
    }

    for (i = 0u; i < 11u; i++) {
        name[i] = ' ';
    }

    while (*filename != '\0') {
        c = (uint8_t)*filename++;

        if (c == '.') {
            if (extension || base_length == 0u) {
                return -1;
            }
            extension = 1;
            continue;
        }

        if (c == ' ' || c == '/' || c == '\\') {
            return -1;
        }

        c = fat_upper(c);

        if (!extension) {
            if (base_length >= 8u) {
                return -1;
            }
            name[base_length++] = c;
        } else {
            if (extension_length >= 3u) {
                return -1;
            }
            name[8u + extension_length++] = c;
        }
    }

    if (base_length == 0u || extension_length == 0u) {
        return -1;
    }

    return 0;
}

// ファイル検索 (ルートディレクトリのクラスタを順番に読み、各32バイトのディレクトリエントリを調べる)
static int fat_find_file(const uint8_t name[11], fat_file *file){
    uint32_t cluster = root_cluster;
    uint32_t sector_index;
    uint32_t entry_index;

    while (cluster >= 2u && cluster < 0x0ffffff8u) {
        for (sector_index = 0u; sector_index < sectors_per_cluster;
             sector_index++) {
            if (sd_read_sector(cluster_lba(cluster) + sector_index,
                               sector_buffer) < 0) {
                return -1;
            }

            for (entry_index = 0u; entry_index < 16u; entry_index++) {
                uint8_t *entry = sector_buffer + entry_index * 32u;
                uint8_t attr = entry[11];

                if (entry[0] == 0x00u) {
                    return 1;
                }
                if (entry[0] == 0xe5u || attr == 0x0fu
                    || (attr & 0x18u) != 0u) {
                    continue;
                }
                if (memcmp(entry, name, 11) == 0) {
                    file->first_cluster =
                        ((uint32_t)get_u16(entry + 20u) << 16)
                        | get_u16(entry + 26u);
                    file->size = get_u32(entry + 28u);
                    return file->first_cluster >= 2u ? 0 : -1;
                }
            }
        }

        if (fat_next_cluster(cluster, &cluster) < 0) {
            return -1;
        }
    }
    return 1;
}

// ファイル内の指定した位置から、指定した長さを読む
static int fat_read(const fat_file *file, uint32_t offset, void *buffer, uint32_t length){
    uint8_t *destination = (uint8_t *)buffer;
    uint32_t cluster_size = sectors_per_cluster * SECTOR_SIZE;
    uint32_t cluster = file->first_cluster;
    uint32_t skip_clusters;
    uint32_t inside_cluster;
    uint32_t i;

    if (offset > file->size || length > file->size - offset) {
        return -1;
    }

    skip_clusters = offset / cluster_size;
    inside_cluster = offset % cluster_size;
    for (i = 0u; i < skip_clusters; i++) {
        if (fat_next_cluster(cluster, &cluster) < 0
            || cluster < 2u || cluster >= 0x0ffffff8u) {
            return -1;
        }
    }

    while (length != 0u) {
        uint32_t sector_index = inside_cluster / SECTOR_SIZE;
        uint32_t inside_sector = inside_cluster % SECTOR_SIZE;
        uint32_t copy_size = SECTOR_SIZE - inside_sector;

        if (copy_size > length) {
            copy_size = length;
        }
        if (sd_read_sector(cluster_lba(cluster) + sector_index,
                           sector_buffer) < 0) {
            return -1;
        }
        memcpy(destination, sector_buffer + inside_sector, (long)copy_size);

        destination += copy_size;
        length -= copy_size;
        inside_cluster += copy_size;

        if (inside_cluster == cluster_size && length != 0u) {
            inside_cluster = 0u;
            if (fat_next_cluster(cluster, &cluster) < 0
                || cluster < 2u || cluster >= 0x0ffffff8u) {
                return -1;
            }
        }
    }
    return 0;
}

/* ---------- ELF loader ---------- */

int sd_elf_load(char *filename, uint32_t *entry){
    uint8_t short_name[11];
    fat_file file;
    elf32_header header;
    elf32_program_header program;
    uint32_t i;
    uint32_t loaded = 0u;
    int result;

    if(entry == NULL){
        return ERR_ELF_HEADER;
    }
    if(fat_make_short_name(filename, short_name) < 0){ // ファイル名をFAT形式へ変換
        return ERR_FILENAME;
    }
    if(sd_init() < 0){ // SDカード初期化
        return ERR_SD_INIT;
    }
    if(fat_mount() < 0){ //FAT32マウント
        return ERR_FAT32;
    }

    result = fat_find_file(short_name, &file); //ファイル検索
    if(result != 0){
        return result > 0 ? ERR_NOT_FOUND : ERR_SD_READ;
    }
    if(fat_read(&file, 0u, &header, sizeof(header)) < 0){ //ELFヘッダー読み取り
        return ERR_ELF_HEADER;
    }

    //LF形式の確認
    //最初にELFマジックナンバーを確認
    if(header.ident[0] != 0x7fu || header.ident[1] != 'E' || header.ident[2] != 'L' || header.ident[3] != 'F'){ // 0x7F, 'E', 'L', 'F'
        return ERR_ELF_HEADER;
    }
    if (header.ident[4] != 1u || header.ident[5] != 1u || header.type != ET_EXEC || header.machine != EM_ARM || header.ehsize != sizeof(header) || header.phentsize != sizeof(program) || header.phnum == 0u || header.phnum > MAX_PHNUM){ // ELF32か, リトルエンディアンか, 実行形式か, ARM向けか確認
        return ERR_ELF_FORMAT;
    }
    if ((header.entry & 1u) == 0u || header.entry < APP_RAM_START || header.entry >= APP_RAM_END){ // エントリが 0x20020000~0x2003FFFF に入っているか確認
        return ERR_ELF_RANGE;
    }

    memset((void *)APP_RAM_START, 0, (long)(APP_RAM_END - APP_RAM_START));

    //Program Headerの処理
    for(i = 0u; i < header.phnum; i++){ // ELFのProgram Headerを1個ずつ読む
        uint32_t end;

        // RAMへコピー
        if (fat_read(&file, header.phoff + i * (uint32_t)header.phentsize, &program, sizeof(program)) < 0) {
            return ERR_ELF_READ;
        }
        if(program.type != PT_LOAD || program.memsz == 0u){ // PT_LOADは実行時にメモリへ置く必要がある (.text, .rodata, .data, .bss)
            continue;
        }
        if (program.filesz > program.memsz || program.vaddr < APP_RAM_START || program.vaddr >= APP_RAM_END || program.memsz > APP_RAM_END - program.vaddr) {
            return ERR_ELF_RANGE;
        }
        end = program.vaddr + program.memsz;
        if (end > APP_RAM_END) {
            return ERR_ELF_RANGE;
        }

        if (program.filesz != 0u && fat_read(&file, program.offset, (void *)program.vaddr, program.filesz) < 0) {
            return ERR_ELF_READ;
        }
        // .bssの初期化
        if (program.memsz > program.filesz) {
            memset((void *)(program.vaddr + program.filesz), 0,
            (long)(program.memsz - program.filesz));
        }
        loaded++;
    }

    if (loaded == 0u) {
        return ERR_NO_SEGMENT;
    }

    // RAMへコードを書き込んだあと、そのコードを実行する前の同期
    __asm__ volatile ("dsb" ::: "memory"); //DSB それまでのメモリ書き込みが完了するまで待つ
    __asm__ volatile ("isb" ::: "memory"); //ISB 命令パイプラインを破棄し、以降の命令を改めて読み直させる
    *entry = header.entry;
    return 0;
}

const char *sd_elf_error(int error){
    switch (error) {
    case ERR_SD_INIT:    return "SD init failed";
    case ERR_SD_READ:    return "SD read failed";
    case ERR_FAT32:      return "FAT32 mount failed";
    case ERR_NOT_FOUND:  return "file not found";
    case ERR_ELF_HEADER: return "invalid ELF header";
    case ERR_ELF_FORMAT: return "unsupported ELF";
    case ERR_ELF_RANGE:  return "ELF is outside app RAM";
    case ERR_ELF_READ:   return "ELF read failed";
    case ERR_NO_SEGMENT: return "no PT_LOAD segment";
    case ERR_FILENAME:   return "invalid FAT 8.3 filename";
    default:             return "unknown error";
    }
}

/*
 * 最小構成:
 *   SPI0  : GP16=MISO, GP17=CS, GP18=SCK, GP19=MOSI
 *   FS    : FAT32のみ
 *   file  : ルートディレクトリのFAT 8.3形式ファイル
 *   ELF   : ELF32 / little endian / ARM / ET_EXEC / PT_LOAD
 *   RAM   : 0x20020000-0x2003ffff (128KB)
*/