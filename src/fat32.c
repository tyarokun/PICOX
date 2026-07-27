#include "define.h"
#include "lib.h"
#include "fat32.h"

static uint16_t get_u16(const uint8_t *p){
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

static uint32_t get_u32(const uint8_t *p){
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static int read_sector(fat32_fs *fs, uint32_t lba){
    if(fs == NULL || fs->read_sector == NULL){
        return -1;
    }
    return fs->read_sector(lba, fs->sector_buffer, fs->read_context);
}

static int looks_like_fat32(const uint8_t *boot){
    return get_u16(boot + 11u) == FAT32_SECTOR_SIZE && boot[13] != 0u && get_u16(boot + 17u) == 0u && get_u16(boot + 22u) == 0u && get_u32(boot + 36u) != 0u && boot[510] == 0x55u && boot[511] == 0xaau;
}

int fat32_mount(fat32_fs *fs, fat32_read_sector_func_t read_sector_func, void *context){
    uint32_t boot_lba = 0u;
    uint32_t reserved;
    uint32_t fat_size;
    uint32_t fat_count;

    if(fs == NULL || read_sector_func == NULL){
        return -1;
    }

    memset(fs, 0, (long)sizeof(*fs));
    fs->read_sector = read_sector_func;
    fs->read_context = context;

    /* セクタ0がFAT32ブートセクタなら、そのまま使用する。 */
    if(read_sector(fs, 0u) < 0){
        return -1;
    }

    /* FAT32でなければMBRとみなし、先頭パーティションを読む。 */
    if(!looks_like_fat32(fs->sector_buffer)){
        boot_lba = get_u32(fs->sector_buffer + 446u + 8u);
        if(boot_lba == 0u || read_sector(fs, boot_lba) < 0 || !looks_like_fat32(fs->sector_buffer)){
            return -1;
        }
    }

    fs->sectors_per_cluster = fs->sector_buffer[13];
    reserved = get_u16(fs->sector_buffer + 14u);
    fat_count = fs->sector_buffer[16];
    fat_size = get_u32(fs->sector_buffer + 36u);
    fs->root_cluster = get_u32(fs->sector_buffer + 44u) & 0x0fffffffu;

    if(fs->sectors_per_cluster == 0u || reserved == 0u || fat_count == 0u || fs->root_cluster < 2u || (fs->sectors_per_cluster & (fs->sectors_per_cluster - 1u)) != 0u){
        return -1;
    }

    fs->fat_start_lba = boot_lba + reserved;
    fs->data_start_lba = fs->fat_start_lba + fat_count * fat_size;
    return 0;
}

static uint32_t cluster_lba(const fat32_fs *fs, uint32_t cluster){
    return fs->data_start_lba + (cluster - 2u) * fs->sectors_per_cluster;
}

static int fat_next_cluster(fat32_fs *fs, uint32_t cluster, uint32_t *next){
    uint32_t offset;

    if(fs == NULL || next == NULL){
        return -1;
    }

    offset = cluster * 4u;
    if(read_sector(fs, fs->fat_start_lba + offset / FAT32_SECTOR_SIZE) < 0){
        return -1;
    }

    *next = get_u32(fs->sector_buffer + offset % FAT32_SECTOR_SIZE) & 0x0fffffffu;
    return 0;
}

static uint8_t fat_upper(uint8_t c){
    if(c >= 'a' && c <= 'z'){
        return (uint8_t)(c - ('a' - 'A'));
    }
    return c;
}

/* FAT 8.3形式の11バイト短縮名へ変換する。 */
static int fat_make_short_name(const char *filename, uint8_t name[11]){
    uint32_t base_length = 0u;
    uint32_t extension_length = 0u;
    int extension = 0;
    uint8_t c;
    uint32_t i;

    if(filename == NULL || *filename == '\0'){
        return -1;
    }

    for(i = 0u; i < 11u; i++){
        name[i] = ' ';
    }

    while(*filename != '\0'){
        c = (uint8_t)*filename++;

        if(c == '.'){
            if(extension || base_length == 0u){
                return -1;
            }
            extension = 1;
            continue;
        }

        if(c == ' ' || c == '/' || c == '\\'){
            return -1;
        }

        c = fat_upper(c);
        if(!extension){
            if(base_length >= 8u){
                return -1;
            }
            name[base_length++] = c;
        }else{
            if(extension_length >= 3u){
                return -1;
            }
            name[8u + extension_length++] = c;
        }
    }

    if(base_length == 0u || extension_length == 0u){
        return -1;
    }
    return 0;
}

static int fat_find_file(fat32_fs *fs, const uint8_t name[11], fat32_file *file){
    uint32_t cluster;
    uint32_t sector_index;
    uint32_t entry_index;

    if(fs == NULL || file == NULL){
        return FAT32_OPEN_IO_ERROR;
    }

    cluster = fs->root_cluster;

    while(cluster >= 2u && cluster < 0x0ffffff8u){
        for(sector_index = 0u;sector_index < fs->sectors_per_cluster; sector_index++){

            if(read_sector(fs,
                cluster_lba(fs, cluster) + sector_index) < 0){
                return FAT32_OPEN_IO_ERROR;
            }

            for(entry_index = 0u; entry_index < 16u; entry_index++){
                uint8_t *entry = fs->sector_buffer + entry_index * 32u;
                uint8_t attr = entry[11];

                if(entry[0] == 0x00u){
                    return FAT32_OPEN_NOT_FOUND;
                }
                if(entry[0] == 0xe5u || attr == 0x0fu || (attr & 0x18u) != 0u){
                    continue;
                }

                if(memcmp(entry, name, 11) == 0){
                    file->fs = fs;
                    file->first_cluster = ((uint32_t)get_u16(entry + 20u) << 16) | get_u16(entry + 26u);
                    file->size = get_u32(entry + 28u);
                    return file->first_cluster >= 2u ? 0 : FAT32_OPEN_IO_ERROR;
                }
            }
        }

        if(fat_next_cluster(fs, cluster, &cluster) < 0){
            return FAT32_OPEN_IO_ERROR;
        }
    }

    return FAT32_OPEN_NOT_FOUND;
}

int fat32_open(fat32_fs *fs, const char *filename, fat32_file *file){
    uint8_t short_name[11];

    if(fs == NULL || file == NULL){
        return FAT32_OPEN_IO_ERROR;
    }
    if(fat_make_short_name(filename, short_name) < 0){
        return FAT32_OPEN_INVALID_NAME;
    }

    memset(file, 0, (long)sizeof(*file));
    return fat_find_file(fs, short_name, file);
}

int fat32_read(const fat32_file *file, uint32_t offset, void *buffer, uint32_t length){
    fat32_fs *fs;
    uint8_t *destination = (uint8_t *)buffer;
    uint32_t cluster_size;
    uint32_t cluster;
    uint32_t skip_clusters;
    uint32_t inside_cluster;
    uint32_t i;

    if(file == NULL || file->fs == NULL || buffer == NULL){
        return -1;
    }
    if(offset > file->size || length > file->size - offset){
        return -1;
    }

    fs = file->fs;
    cluster_size =
        fs->sectors_per_cluster * FAT32_SECTOR_SIZE;
    cluster = file->first_cluster;
    skip_clusters = offset / cluster_size;
    inside_cluster = offset % cluster_size;

    for(i = 0u; i < skip_clusters; i++){
        if(fat_next_cluster(fs, cluster, &cluster) < 0 || cluster < 2u || cluster >= 0x0ffffff8u){
            return -1;
        }
    }

    while(length != 0u){
        uint32_t sector_index =
            inside_cluster / FAT32_SECTOR_SIZE;
        uint32_t inside_sector =
            inside_cluster % FAT32_SECTOR_SIZE;
        uint32_t copy_size =
            FAT32_SECTOR_SIZE - inside_sector;

        if(copy_size > length){
            copy_size = length;
        }

        if(read_sector(fs,
            cluster_lba(fs, cluster) + sector_index) < 0){
            return -1;
        }

        memcpy(destination,
               fs->sector_buffer + inside_sector,
               (long)copy_size);

        destination += copy_size;
        length -= copy_size;
        inside_cluster += copy_size;

        if(inside_cluster == cluster_size && length != 0u){
            inside_cluster = 0u;
            if(fat_next_cluster(fs, cluster, &cluster) < 0 || cluster < 2u || cluster >= 0x0ffffff8u){
                return -1;
            }
        }
    }
    return 0;
}
