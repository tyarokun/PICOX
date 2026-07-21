#ifndef CONSDRV_H
#define CONSDRV_H

#define CONSDRV_DEVICE_NUM       1
#define CONSDRV_DEFAULT_DEVICE   0

/*
 * One message must fit in PICOX's current 64-byte memory pool.
 * The allocator header occupies part of the block, so keep payloads small.
 */
#define CONSDRV_WRITE_CHUNK_SIZE 48

typedef enum {
    CONSDRV_CMD_USE = 0,
    CONSDRV_CMD_WRITE,
} consdrv_command_t;

int consdrv_main(int argc, char *argv[]);

#endif