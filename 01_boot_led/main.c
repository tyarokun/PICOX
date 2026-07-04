#define TIMERAWL      0x40054028

#define GPIO25_CTL    0x400140cc  // GPIO25
#define GPIO_OUT_SET  0xd0000014
#define GPIO_OUT_CLR  0xd0000018
#define GPIO_OUT_OE   0xd0000020

#define GPIO25_MASK   0x02000000  // 1 << 25

unsigned int get_system_count()
{
    return *(volatile unsigned int *)TIMERAWL;
}

void wait(unsigned int msec)
{
    unsigned int wait_count = msec * 1000;
    unsigned int start = get_system_count();

    while (get_system_count() - start < wait_count);
}

int main(void){
    /*
     * GPIO25をSIOブロックから制御する設定
     * FUNCSEL = 5 が SIO
     */
    *(volatile unsigned int *)GPIO25_CTL = 0x00000005;

    /*
     * GPIO25をoutputに設定
     * bit25 = 1
     */
    *(volatile unsigned int *)GPIO_OUT_OE = GPIO25_MASK;

    while (1) {
        /*
         * GPIO25をHIGHにする
         */
        *(volatile unsigned int *)GPIO_OUT_SET = GPIO25_MASK;
        wait(500);

        /*
         * GPIO25をLOWにする
         */
        *(volatile unsigned int *)GPIO_OUT_CLR = GPIO25_MASK;
        wait(500);
    }
}