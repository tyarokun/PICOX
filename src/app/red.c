#define TIMERAWL         0x40054028u

#define GPIO5_CTRL       0x4001402Cu
#define SIO_GPIO_OUT_SET 0xD0000014u
#define SIO_GPIO_OUT_CLR 0xD0000018u
#define SIO_GPIO_OE_SET  0xD0000024u

#define GPIO5_MASK       (1u << 5)

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



int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;


    /* GPIO5のファンクションをSIO (5) に設定 */
    *(volatile unsigned int *)GPIO5_CTRL = 5u;

    /* GPIO5を出力有効（Output Enable）に設定 */
    *(volatile unsigned int *)SIO_GPIO_OE_SET = GPIO5_MASK;

    for(;;){
        *(volatile unsigned int *)SIO_GPIO_OUT_SET = GPIO5_MASK;
        wait_msec(500u);

        *(volatile unsigned int *)SIO_GPIO_OUT_CLR = GPIO5_MASK;
        wait_msec(500u);
    }

    return 0;
}