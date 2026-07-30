#include "app_memory.h"
#include "lib.h"

static uint8_t page_used[APP_PAGE_COUNT];

void app_memory_init(void){
    memset(page_used, 0, sizeof(page_used));
}

static int is_power_of_two(uint32_t value){
    return value != 0u && (value & (value - 1)) == 0;
}

int app_memory_alloc(uint32_t size, uint32_t alignment, app_memory_region *region){
    uint32_t required_pages;
    uint32_t first;
    uint32_t i;
    uint32_t base;

    if(size == 0 || region == NULL){
        return -1;
    }
    if(alignment == 0){
        alignment = 4;
    }
    if(!is_power_of_two(alignment)){
        return -1;
    }
    required_pages = (size + APP_PAGE_SIZE - 1) / APP_PAGE_SIZE;

    if(required_pages > APP_PAGE_COUNT){
        return -1;
    }

    // First-fit: 先頭から、必要ページ数だけ連続して空いている場所を探す
    for (first = 0; first + required_pages <= APP_PAGE_COUNT; first++){
        base = APP_RAM_START + first * APP_PAGE_SIZE;
        if((base & (alignment - 1)) != 0){
            continue;
        }
        for(i = 0u; i < required_pages; i++){
            if (page_used[first + i]) {
                break;
            }
        }
        if(i != required_pages){
            continue;
        }
        for(i = 0u; i < required_pages; i++){
            page_used[first + i] = 1;
        }
        region->base = base;
        region->size = required_pages * APP_PAGE_SIZE;
        region->first_page = (uint16_t)first;
        region->page_count = (uint16_t)required_pages;
        return 0;
    }
    return -1;
}

void app_memory_free(app_memory_region *region){
    uint32_t i;
    if(region == NULL || region->page_count == 0){
        return;
    }
    for(i = 0u; i < region->page_count; i++){
        uint32_t page = region->first_page + i;
        if (page < APP_PAGE_COUNT) {
            page_used[page] = 0;
        }
    }
    memset((void *)region->base, 0, region->size);
    memset(region, 0, sizeof(*region));
}