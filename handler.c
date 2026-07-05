#include "clock.h"
#include "handler.h"


void Reset_Handler(void){
    clock_init_125mhz();
    main();
    while(1);
}

void Default_Handler(void){
    while(1);
}