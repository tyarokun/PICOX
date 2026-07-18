#ifndef MEMORY_H
#define MEMORY_H

int picoxmem_init(void);        /* 動的メモリの初期化 */
void *picoxmem_alloc(int size); /* 動的メモリの獲得 */
void picoxmem_free(void *mem);  /* メモリの解放 */

#endif