#include "handler.h"

void Reset_Handler(void){
    main();
    while(1);
}

void Default_Handler(void){
    while(1);
}