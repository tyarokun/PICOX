#define TIMERAWL          0x40054028u

#define GPIO25_CTRL       0x400140CCu
#define SIO_GPIO_OUT_SET  0xD0000014u
#define SIO_GPIO_OUT_CLR  0xD0000018u
#define SIO_GPIO_OE_SET   0xD0000024u

#define GPIO25_MASK       (1u << 25)

static unsigned int get_system_count(void)
{
    return *(volatile unsigned int *)TIMERAWL;
}

static void wait_msec(unsigned int msec)
{
    unsigned int wait_count = msec * 1000u;
    unsigned int start = get_system_count();

    while ((unsigned int)(get_system_count() - start) < wait_count) {
        /* busy wait */
    }
}

volatile int interval = 500u;
volatile int count;
volatile char a[100];

int main(int argc, char *argv[])
{
    *(volatile unsigned int *)GPIO25_CTRL = 5u;
    *(volatile unsigned int *)SIO_GPIO_OE_SET = GPIO25_MASK;
    count = 0;

    for(;;){
        interval++;
        a[count++] = count;
        *(volatile unsigned int *)SIO_GPIO_OUT_SET = GPIO25_MASK;
        wait_msec(interval);

        *(volatile unsigned int *)SIO_GPIO_OUT_CLR = GPIO25_MASK;
        wait_msec(interval);
        if(count > 100){
            interval -= 100;
            count = 5;
        }
    }

    return 0;
}
