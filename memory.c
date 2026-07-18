#include "define.h"
#include "memory.h"
#include "lib.h"
#include "kernel.h"

typedef struct _picoxmem_block{
    struct _picoxmem_block *next;
    int size;
}picoxmem_block;

typedef struct _picoxmem_pool{
    int size;
    int num;
    picoxmem_block *free;
}picoxmem_pool;

static picoxmem_pool pool[] = {
    {16, 8, NULL},
    {32, 8, NULL},
    {64, 4, NULL},
};

#define MEMORY_AREA_NUM (sizeof(pool) / sizeof(*pool))

static int picoxmem_init_pool(picoxmem_pool *p){
    extern char _freearea;
    static char *area = &_freearea;
    int i;
    picoxmem_block *mp;
    picoxmem_block **mpp;
    mp = (picoxmem_block *)area;
    mpp = &p->free;
    for(i = 0; i < p->num; i++){
        *mpp = mp;
        *memset(mp, 0, sizeof(*mp));
        mp->size = p->size;
        mpp = &(mp->next);
        mp = (picoxmem_block *)((char *)mp + p->size);
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
    picoxmem_block *mp;
    picoxmem_pool *p;
    for(i = 0; i < MEMORY_AREA_NUM; i++){
        p = &pool[i];
    if(size <= p->size - sizeof(picoxmem_block)){
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
    picoxmem_block *mp;
    picoxmem_pool *p;

    /* 領域の直前にある(はずの)メモリ・ブロック構造体を取得 */
    mp = ((picoxmem_block *)mem - 1);

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