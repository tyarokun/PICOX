#ifndef CONSDRV_H
#define CONSDRV_H

int consdrv_main(int argc, char *argv[]);
int consdrv_write(char *text);
void consdrv_write_int(int value);

#endif