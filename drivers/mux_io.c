#include "mux_io.h"

#include "stm32f1xx.h"

static void mux_all_low(void)
{
    GPIOC->BSRR = GPIO_BSRR_BR9;
    GPIOA->BSRR = GPIO_BSRR_BR8 | GPIO_BSRR_BR11 | GPIO_BSRR_BR12;
}

void mux_init(void)
{
    RCC->APB2ENR |= RCC_APB2ENR_IOPCEN | RCC_APB2ENR_IOPAEN;

    // MUX1 -> PC9
    GPIOC->CRH &= ~(GPIO_CRH_MODE9 | GPIO_CRH_CNF9);
    GPIOC->CRH |= GPIO_CRH_MODE9;

    // MUX2 -> PA8
    GPIOA->CRH &= ~(GPIO_CRH_MODE8 | GPIO_CRH_CNF8);
    GPIOA->CRH |= GPIO_CRH_MODE8;

    // MUX3 -> PA11
    GPIOA->CRH &= ~(GPIO_CRH_MODE11 | GPIO_CRH_CNF11);
    GPIOA->CRH |= GPIO_CRH_MODE11;

    // MUX4 -> PA12
    GPIOA->CRH &= ~(GPIO_CRH_MODE12 | GPIO_CRH_CNF12);
    GPIOA->CRH |= GPIO_CRH_MODE12;

    mux_all_low();
}

void mux_select(uint8_t mux_id)
{
    // Enforce one-hot output: selected mux line high, all others low.
    mux_all_low();

    if (mux_id == 1u) {
        GPIOC->BSRR = GPIO_BSRR_BS9;
    } else if (mux_id == 2u) {
        GPIOA->BSRR = GPIO_BSRR_BS8;
    } else if (mux_id == 3u) {
        GPIOA->BSRR = GPIO_BSRR_BS11;
    } else if (mux_id == 4u) {
        GPIOA->BSRR = GPIO_BSRR_BS12;
    }
}
