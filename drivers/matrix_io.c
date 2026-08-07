#include "matrix_io.h"

#include "stm32f1xx.h"

static void matrix_apply_row_mask(uint16_t row_mask)
{
    if (row_mask & (1u << 0)) {
        GPIOC->BSRR = GPIO_BSRR_BS1;
    } else {
        GPIOC->BSRR = GPIO_BSRR_BR1;
    }

    if (row_mask & (1u << 1)) {
        GPIOC->BSRR = GPIO_BSRR_BS2;
    } else {
        GPIOC->BSRR = GPIO_BSRR_BR2;
    }

    if (row_mask & (1u << 2)) {
        GPIOC->BSRR = GPIO_BSRR_BS3;
    } else {
        GPIOC->BSRR = GPIO_BSRR_BR3;
    }

    if (row_mask & (1u << 3)) {
        GPIOA->BSRR = GPIO_BSRR_BS1;
    } else {
        GPIOA->BSRR = GPIO_BSRR_BR1;
    }
}

static void matrix_apply_col_mask(uint16_t col_mask)
{
    if (col_mask & (1u << 0)) {
        GPIOA->BSRR = GPIO_BSRR_BS2;
    } else {
        GPIOA->BSRR = GPIO_BSRR_BR2;
    }

    if (col_mask & (1u << 1)) {
        GPIOA->BSRR = GPIO_BSRR_BS4;
    } else {
        GPIOA->BSRR = GPIO_BSRR_BR4;
    }

    if (col_mask & (1u << 2)) {
        GPIOA->BSRR = GPIO_BSRR_BS5;
    } else {
        GPIOA->BSRR = GPIO_BSRR_BR5;
    }

    if (col_mask & (1u << 3)) {
        GPIOA->BSRR = GPIO_BSRR_BS6;
    } else {
        GPIOA->BSRR = GPIO_BSRR_BR6;
    }

    if (col_mask & (1u << 4)) {
        GPIOA->BSRR = GPIO_BSRR_BS7;
    } else {
        GPIOA->BSRR = GPIO_BSRR_BR7;
    }

    if (col_mask & (1u << 5)) {
        GPIOC->BSRR = GPIO_BSRR_BS4;
    } else {
        GPIOC->BSRR = GPIO_BSRR_BR4;
    }

    if (col_mask & (1u << 6)) {
        GPIOC->BSRR = GPIO_BSRR_BS5;
    } else {
        GPIOC->BSRR = GPIO_BSRR_BR5;
    }

    if (col_mask & (1u << 7)) {
        GPIOB->BSRR = GPIO_BSRR_BS0;
    } else {
        GPIOB->BSRR = GPIO_BSRR_BR0;
    }

    if (col_mask & (1u << 8)) {
        GPIOB->BSRR = GPIO_BSRR_BS1;
    } else {
        GPIOB->BSRR = GPIO_BSRR_BR1;
    }
}

void matrix_init(void)
{
    RCC->APB2ENR |= RCC_APB2ENR_IOPCEN | RCC_APB2ENR_IOPAEN | RCC_APB2ENR_IOPBEN;

    GPIOC->CRL &= ~(GPIO_CRL_MODE1 | GPIO_CRL_CNF1 |
                    GPIO_CRL_MODE2 | GPIO_CRL_CNF2 |
                    GPIO_CRL_MODE3 | GPIO_CRL_CNF3 |
                    GPIO_CRL_MODE4 | GPIO_CRL_CNF4 |
                    GPIO_CRL_MODE5 | GPIO_CRL_CNF5);
    GPIOC->CRL |= (GPIO_CRL_MODE1 | GPIO_CRL_MODE2 |
                   GPIO_CRL_MODE3 | GPIO_CRL_MODE4 | GPIO_CRL_MODE5);

    GPIOA->CRL &= ~(GPIO_CRL_MODE1 | GPIO_CRL_CNF1 |
                    GPIO_CRL_MODE2 | GPIO_CRL_CNF2 |
                    GPIO_CRL_MODE4 | GPIO_CRL_CNF4 |
                    GPIO_CRL_MODE5 | GPIO_CRL_CNF5 |
                    GPIO_CRL_MODE6 | GPIO_CRL_CNF6 |
                    GPIO_CRL_MODE7 | GPIO_CRL_CNF7);
    GPIOA->CRL |= (GPIO_CRL_MODE1 | GPIO_CRL_MODE2 |
                   GPIO_CRL_MODE4 | GPIO_CRL_MODE5 |
                   GPIO_CRL_MODE6 | GPIO_CRL_MODE7);

    GPIOB->CRL &= ~(GPIO_CRL_MODE0 | GPIO_CRL_CNF0 |
                    GPIO_CRL_MODE1 | GPIO_CRL_CNF1);
    GPIOB->CRL |= (GPIO_CRL_MODE0 | GPIO_CRL_MODE1);

    matrix_apply_masks(0u, 0u);
}

void matrix_apply_masks(uint16_t row_mask, uint16_t col_mask)
{
    uint16_t valid_row_mask = (uint16_t)((1u << MATRIX_ROW_COUNT) - 1u);
    uint16_t valid_col_mask = (uint16_t)((1u << MATRIX_COL_COUNT) - 1u);

    row_mask &= valid_row_mask;
    col_mask &= valid_col_mask;

    matrix_apply_row_mask(row_mask);
    matrix_apply_col_mask(col_mask);
}
