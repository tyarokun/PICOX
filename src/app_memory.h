#ifndef APP_MEMORY_H
#define APP_MEMORY_H

#include "define.h"

#define APP_RAM_START 0x20020000
#define APP_RAM_END   0x20040000

#define APP_PAGE_SIZE  256
#define APP_PAGE_COUNT ((APP_RAM_END - APP_RAM_START) / APP_PAGE_SIZE)

typedef struct {
    uint32_t base;
    uint32_t size;
    uint16_t first_page;
    uint16_t page_count;
} app_memory_region;

void app_memory_init(void);
int app_memory_alloc(uint32_t size, uint32_t alignment, app_memory_region *region);
void app_memory_free(app_memory_region *region);

#endif