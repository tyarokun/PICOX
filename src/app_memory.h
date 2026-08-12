#ifndef APP_MEMORY_H
#define APP_MEMORY_H

#include "define.h"

//#define APP_RAM_START 0x20020000
//#define APP_RAM_END   0x20040000
extern uint32_t _app_start, _app_end;

#define APP_PAGE_SIZE  256
// #define APP_PAGE_COUNT ((_app_end - _app_start) / APP_PAGE_SIZE)
#define APP_PAGE_COUNT 512

typedef struct {
    uint32_t base;          // 確保した実アドレス
    uint32_t size;          // 実際に確保したサイズ
    uint16_t first_page;    // 最初のページ番号
    uint16_t page_count;    // 確保したページ数
} app_memory_region;

void app_memory_init(void);
int app_memory_alloc(uint32_t size, uint32_t alignment, app_memory_region *region);
void app_memory_free(app_memory_region *region);

#endif