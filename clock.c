#include <stdint.h>
#include "clock.h"

#define REG32(addr) (*(volatile uint32_t *)(addr))

/* ===== Base addresses ===== */
#define RESETS_BASE   0x4000C000u
#define XOSC_BASE     0x40024000u
#define CLOCKS_BASE   0x40008000u
#define PLL_SYS_BASE  0x40028000u

/* ===== RESETS ===== */
#define RESETS_RESET       REG32(RESETS_BASE + 0x00)
#define RESETS_RESET_DONE  REG32(RESETS_BASE + 0x08)

#define RESET_PLL_SYS      (1u << 12)

/* ===== XOSC ===== */
#define XOSC_CTRL       REG32(XOSC_BASE + 0x00)
#define XOSC_STATUS     REG32(XOSC_BASE + 0x04)
#define XOSC_STARTUP    REG32(XOSC_BASE + 0x0C)

#define XOSC_CTRL_FREQ_RANGE_1_15MHZ  0xAA0u
#define XOSC_CTRL_ENABLE              (0xFABu << 12)
#define XOSC_STATUS_STABLE            (1u << 31)

/* ===== CLOCKS ===== */
#define CLK_REF_CTRL       REG32(CLOCKS_BASE + 0x30)
#define CLK_REF_DIV        REG32(CLOCKS_BASE + 0x34)
#define CLK_REF_SELECTED   REG32(CLOCKS_BASE + 0x38)

#define CLK_SYS_CTRL       REG32(CLOCKS_BASE + 0x3C)
#define CLK_SYS_DIV        REG32(CLOCKS_BASE + 0x40)
#define CLK_SYS_SELECTED   REG32(CLOCKS_BASE + 0x44)

#define CLK_PERI_CTRL      REG32(CLOCKS_BASE + 0x48)

/* clk_ref source */
#define CLK_REF_SRC_ROSC   0u
#define CLK_REF_SRC_AUX    1u
#define CLK_REF_SRC_XOSC   2u

/* clk_sys */
#define CLK_SYS_SRC_CLK_REF   0u
#define CLK_SYS_SRC_AUX       1u

#define CLK_SYS_AUXSRC_PLL_SYS  0u

/* clk_peri */
#define CLK_PERI_ENABLE         (1u << 11)
#define CLK_PERI_AUXSRC_CLK_SYS 0u

/* ===== PLL_SYS ===== */
#define PLL_SYS_CS         REG32(PLL_SYS_BASE + 0x00)
#define PLL_SYS_PWR        REG32(PLL_SYS_BASE + 0x04)
#define PLL_SYS_FBDIV_INT  REG32(PLL_SYS_BASE + 0x08)
#define PLL_SYS_PRIM       REG32(PLL_SYS_BASE + 0x0C)

#define PLL_CS_LOCK        (1u << 31)

#define PLL_PWR_PD         (1u << 0)
#define PLL_PWR_POSTDIVPD  (1u << 3)
#define PLL_PWR_VCOPD      (1u << 5)

#define PLL_PRIM_POSTDIV1_LSB  16
#define PLL_PRIM_POSTDIV2_LSB  12

static void xosc_init_12mhz(void)
{
    XOSC_CTRL = XOSC_CTRL_FREQ_RANGE_1_15MHZ;
    /*
     * Picoの12MHz水晶用。
     * 0xC4はよく使われる起動待ち値。
     */
    XOSC_STARTUP = 0xC4u;
    XOSC_CTRL = XOSC_CTRL_ENABLE | XOSC_CTRL_FREQ_RANGE_1_15MHZ;
    while ((XOSC_STATUS & XOSC_STATUS_STABLE) == 0) {
        /* wait */
    }
}

static void clk_ref_from_xosc(void)
{
    CLK_REF_DIV = 1u << 8;      /* divide by 1 */
    CLK_REF_CTRL = CLK_REF_SRC_XOSC;

    while ((CLK_REF_SELECTED & (1u << CLK_REF_SRC_XOSC)) == 0) {
        /* wait */
    }
}

static void clk_sys_from_clk_ref(void)
{
    CLK_SYS_DIV = 1u << 8;
    CLK_SYS_CTRL = CLK_SYS_SRC_CLK_REF;

    while ((CLK_SYS_SELECTED & (1u << CLK_SYS_SRC_CLK_REF)) == 0) {
        /* wait */
    }
}

static void pll_sys_init_125mhz(void)
{
    /*
     * 念のためPLL_SYSをリセットしてから設定する。
     */
    RESETS_RESET |= RESET_PLL_SYS;
    RESETS_RESET &= ~RESET_PLL_SYS;

    while ((RESETS_RESET_DONE & RESET_PLL_SYS) == 0) {
        /* wait */
    }

    /*
     * PLL SYS:
     * ref = 12MHz
     * refdiv = 1
     * fbdiv = 125
     * VCO = 12MHz * 125 = 1500MHz
     * postdiv1 = 6
     * postdiv2 = 2
     * output = 1500MHz / 6 / 2 = 125MHz
     */
    PLL_SYS_CS = 1u;             /* REFDIV = 1 */
    PLL_SYS_FBDIV_INT = 125u;    /* FBDIV = 125 */

    /*
     * PLL本体とVCOをON。
     * post dividerはlock後にONにする。
     */
    PLL_SYS_PWR &= ~(PLL_PWR_PD | PLL_PWR_VCOPD);

    while ((PLL_SYS_CS & PLL_CS_LOCK) == 0) {
        /* wait */
    }

    PLL_SYS_PRIM =
        (6u << PLL_PRIM_POSTDIV1_LSB) |
        (2u << PLL_PRIM_POSTDIV2_LSB);

    PLL_SYS_PWR &= ~PLL_PWR_POSTDIVPD;
}

static void clk_sys_from_pll_sys_125mhz(void)
{
    CLK_SYS_DIV = 1u << 8;   /* divide by 1 */

    /*
     * AUXSRC = PLL_SYS
     * SRC    = AUX
     */
    CLK_SYS_CTRL =
        (CLK_SYS_AUXSRC_PLL_SYS << 5) |
        CLK_SYS_SRC_AUX;

    while ((CLK_SYS_SELECTED & (1u << CLK_SYS_SRC_AUX)) == 0) {
        /* wait */
    }
}

static void clk_peri_from_clk_sys(void)
{
    /*
     * UARTなどの周辺機器クロックをclk_sysにする。
     * つまり clk_peri = 125MHz。
     */
    CLK_PERI_CTRL =
        CLK_PERI_ENABLE |
        (CLK_PERI_AUXSRC_CLK_SYS << 5);
}

void clock_init_125mhz(void)
{
    xosc_init_12mhz();

    /*
     * まず安全にclk_refをXOSCへ。
     */
    clk_ref_from_xosc();

    /*
     * PLL_SYSをいじる前に、clk_sysをclk_refへ逃がす。
     */
    clk_sys_from_clk_ref();

    /*
     * PLL_SYSを125MHzに設定。
     */
    pll_sys_init_125mhz();

    /*
     * clk_sys = PLL_SYS = 125MHz。
     */
    clk_sys_from_pll_sys_125mhz();

    /*
     * clk_peri = clk_sys = 125MHz。
     * UARTのボーレート計算はこの125MHzを使う。
     */
    clk_peri_from_clk_sys();
}