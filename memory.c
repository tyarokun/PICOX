#include "define.h"
#include "memory.h"
#include "lib.h"
#include "kernel.h"

typedef struct _mem_block{
    struct _mem_block *next;
    int size;
}mem_header;

typedef struct _mem_pool{
    int size;
    int num;
    mem_header *free;
}mem_pool;

static mem_pool pool[] = {
    {16, 8, NULL},
    {32, 8, NULL},
    {64, 4, NULL},
};

#define MEMORY_AREA_NUM (sizeof(pool) / sizeof(*pool))

static int picoxmem_init_pool(mem_pool *p){
    extern char _freearea;
    static char *area = &_freearea;
    int i;
    mem_header *mp;
    mem_header **mpp;
    mp = (mem_header *)area;
    mpp = &p->free;
    for(i = 0; i < p->num; i++){
        *mpp = mp;
        *memset(mp, 0, sizeof(*mp));
        mp->size = p->size;
        mpp = &(mp->next);
        mp = (mem_header *)((char *)mp + p->size);
        area += p->size;
    }
    return 0;
}

int picoxmem_init(void){
    int i;
    for (i = 0; i < MEMORY_AREA_NUM; i++) {
        picoxmem_init_pool(&pool[i]); /* 各メモリ・プールを初期化する */
    }
    return 0;
}

void *picoxmem_alloc(int size){
    int i;
    mem_header *mp;
    mem_pool *p;
    for(i = 0; i < MEMORY_AREA_NUM; i++){
        p = &pool[i];
    if(size <= p->size - sizeof(mem_header)){
        if(p->free == NULL){ /* 解放済み領域が無い(メモリ・ブロック不足) */
	        picox_sysdown();
	        return NULL;
        }
        /* 解放済みリンクリストから領域を取得する */
        mp = p->free;
        p->free = p->free->next;
        mp->next = NULL;
        /*
        * 実際に利用可能な領域は，メモリ・ブロック構造体の直後の領域に
        * なるので，直後のアドレスを返す．
        */
        return mp + 1;
    }
  }
  /* 指定されたサイズの領域を格納できるメモリ・プールが無い */
  picox_sysdown();
  return NULL;
}

void picoxmem_free(void *mem){
    int i;
    mem_header *mp;
    mem_pool *p;

    /* 領域の直前にある(はずの)メモリ・ブロック構造体を取得 */
    mp = ((mem_header *)mem - 1);

    for (i = 0; i < MEMORY_AREA_NUM; i++) {
      p = &pool[i];
      if (mp->size == p->size) {
        /* 領域を解放済みリンクリストに戻す */
        mp->next = p->free;
        p->free = mp;
        return;
      }
    }
    picox_sysdown();
}