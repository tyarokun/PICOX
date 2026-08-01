#include "app_memory.h"
#include "lib.h"

static char page_used[APP_PAGE_COUNT];   // 使用されているページの管理(0:空き, 1:使用中)

// 全ページ空き状態
void app_memory_init(void){
    memset(page_used, 0, sizeof(page_used));
}

// アラインメントが2の累乗か確認
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

    required_pages = (size + APP_PAGE_SIZE - 1) / APP_PAGE_SIZE; //必要ページ数は切り上げ除算で計算

    if(required_pages > APP_PAGE_COUNT){
        return -1;
    }

    // 先頭から、必要ページ数だけ連続して空いている場所を探す
    for (first = 0; first + required_pages <= APP_PAGE_COUNT; first++){
        base = APP_RAM_START + first * APP_PAGE_SIZE;
        if((base & (alignment - 1)) != 0){ // アラインメントの確認 (候補となる先頭アドレスが、ELFの要求する境界にそろっているか確認)
            continue;
        }
        for(i = 0; i < required_pages; i++){ // 連続した空きページを確保
            if(page_used[first + i]){
                break;
            }
        }
        if(i != required_pages){
            continue;
        }
        for(i = 0; i < required_pages; i++){ //確保するページを使用中に変更
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