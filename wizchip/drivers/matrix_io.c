#include "matrix_io.h"
#include "stm32f1xx.h"
#include "delay.h"

#define MATRIX_MAX_PAIRS 9u
#define MATRIX_PAIR_DWELL_US 1200u

// static matrix_pair_t matrix_pairs[MATRIX_MAX_PAIRS];
static uint8_t matrix_pair_count = 0u;
static void matrix_apply_col_mask(uint16_t col_mask)
{
    /*
     * C1 -> PA2
     */
    if (col_mask & (1u << 0))
    {
        GPIOA->BSRR = GPIO_BSRR_BS2;
    }
    else
    {
        GPIOA->BSRR = GPIO_BSRR_BR2;
    }

    /*
     * C2 -> PA4
     */
    if (col_mask & (1u << 1))
    {
        GPIOA->BSRR = GPIO_BSRR_BS4;
    }
    else
    {
        GPIOA->BSRR = GPIO_BSRR_BR4;
    }

    /*
     * C3 -> PA5
     */
    if (col_mask & (1u << 2))
    {
        GPIOA->BSRR = GPIO_BSRR_BS5;
    }
    else
    {
        GPIOA->BSRR = GPIO_BSRR_BR5;
    }

    /*
     * C4 -> PA6
     */
    if (col_mask & (1u << 3))
    {
        GPIOA->BSRR = GPIO_BSRR_BS6;
    }
    else
    {
        GPIOA->BSRR = GPIO_BSRR_BR6;
    }

    /*
     * C5 -> PA7
     */
    if (col_mask & (1u << 4))
    {
        GPIOA->BSRR = GPIO_BSRR_BS7;
    }
    else
    {
        GPIOA->BSRR = GPIO_BSRR_BR7;
    }

    /*
     * C6 -> PC4
     */
    if (col_mask & (1u << 5))
    {
        GPIOC->BSRR = GPIO_BSRR_BS4;
    }
    else
    {
        GPIOC->BSRR = GPIO_BSRR_BR4;
    }

    /*
     * C7 -> PC5
     */
    if (col_mask & (1u << 6))
    {
        GPIOC->BSRR = GPIO_BSRR_BS5;
    }
    else
    {
        GPIOC->BSRR = GPIO_BSRR_BR5;
    }

    /*
     * C8 -> PB0
     */
    if (col_mask & (1u << 7))
    {
        GPIOB->BSRR = GPIO_BSRR_BS0;
    }
    else
    {
        GPIOB->BSRR = GPIO_BSRR_BR0;
    }

    /*
     * C9 -> PB1
     */
    if (col_mask & (1u << 8))
    {
        GPIOB->BSRR = GPIO_BSRR_BS1;
    }
    else
    {
        GPIOB->BSRR = GPIO_BSRR_BR1;
    }
}



static uint8_t row_1(uint16_t col_bits)
{       // R1 -> PC1
    uint8_t prntbuf[80];
    sprintf(prntbuf, "row routine:: row id ->1 , column bits -> %x\r\n",  col_bits);
        uart1_print(prntbuf);
      GPIOC->BSRR = GPIO_BSRR_BS1; 
        matrix_apply_col_mask(col_bits);
         _us_delay(50);
      GPIOC->BSRR = GPIO_BSRR_BR1; 
    return 0;
}

static uint8_t row_2(uint16_t col_bits)
{
        // R2 -> PC2
    uint8_t prntbuf[80];
    sprintf(prntbuf, "row routine:: row id ->2 , column bits -> %x\r\n",  col_bits);
        uart1_print(prntbuf);
            GPIOC->BSRR = GPIO_BSRR_BS2;
        matrix_apply_col_mask(col_bits);
       _us_delay(50);
       GPIOC->BSRR = GPIO_BSRR_BR2;
    return 0;
}

static uint8_t row_3(uint16_t col_bits)
{
               // R3 -> PC3
        uint8_t prntbuf[80];
    sprintf(prntbuf, "row routine:: row id ->3 , column bits -> %x\r\n",  col_bits);
        uart1_print(prntbuf);
           GPIOC->BSRR = GPIO_BSRR_BS3;
    matrix_apply_col_mask(col_bits);
        _us_delay(50);
    GPIOC->BSRR = GPIO_BSRR_BR3;
    return 0;
}

static uint8_t row_4(uint16_t col_bits)
{
        uint8_t prntbuf[80];
    sprintf(prntbuf, "row routine:: row id ->4 , column bits -> %x\r\n",  col_bits);
        uart1_print(prntbuf);
          GPIOA->BSRR = GPIO_BSRR_BS1;
    matrix_apply_col_mask(col_bits);
      _us_delay(50);
    GPIOA->BSRR = GPIO_BSRR_BR1;
     return 0;
}



static uint8_t (*rowfptr[])(uint16_t ) = {&row_1, &row_2, &row_3, &row_4};



void matrix_init(void)
{
    RCC->APB2ENR |=
        RCC_APB2ENR_IOPCEN |
        RCC_APB2ENR_IOPAEN |
        RCC_APB2ENR_IOPBEN;

    /*
     * PC1 = R1
     * PC2 = R2
     * PC3 = R3
     * PC4 = C6
     * PC5 = C7
     */

    GPIOC->CRL &= ~(
        GPIO_CRL_MODE1 | GPIO_CRL_CNF1 |
        GPIO_CRL_MODE2 | GPIO_CRL_CNF2 |
        GPIO_CRL_MODE3 | GPIO_CRL_CNF3 |
        GPIO_CRL_MODE4 | GPIO_CRL_CNF4 |
        GPIO_CRL_MODE5 | GPIO_CRL_CNF5);

    GPIOC->CRL |= (GPIO_CRL_MODE1 |
                   GPIO_CRL_MODE2 |
                   GPIO_CRL_MODE3 |
                   GPIO_CRL_MODE4 |
                   GPIO_CRL_MODE5);

    /*
     * PA1 = R4
     * PA2 = C1
     * PA4 = C2
     * PA5 = C3
     * PA6 = C4
     * PA7 = C5
     */

    GPIOA->CRL &= ~(
        GPIO_CRL_MODE1 | GPIO_CRL_CNF1 |
        GPIO_CRL_MODE2 | GPIO_CRL_CNF2 |
        GPIO_CRL_MODE4 | GPIO_CRL_CNF4 |
        GPIO_CRL_MODE5 | GPIO_CRL_CNF5 |
        GPIO_CRL_MODE6 | GPIO_CRL_CNF6 |
        GPIO_CRL_MODE7 | GPIO_CRL_CNF7);

    GPIOA->CRL |= (GPIO_CRL_MODE1 |
                   GPIO_CRL_MODE2 |
                   GPIO_CRL_MODE4 |
                   GPIO_CRL_MODE5 |
                   GPIO_CRL_MODE6 |
                   GPIO_CRL_MODE7);

    /*
     * PB0 = C8
     * PB1 = C9
     */

    GPIOB->CRL &= ~(
        GPIO_CRL_MODE0 | GPIO_CRL_CNF0 |
        GPIO_CRL_MODE1 | GPIO_CRL_CNF1);

    GPIOB->CRL |= (GPIO_CRL_MODE0 |
                   GPIO_CRL_MODE1);

    /*
     * Safe initial state.
     */
    //matrix_apply_row_mask(0u);
    //matrix_apply_col_mask(0u);

    matrix_pair_count = 0u;

    // Used by pair scanner dwell timing.
    TIM2_Init();
}

void matrix_apply_pairs(
    uint16_t *col_config)
{
    if (col_config == 0 || col_config == 0u)
    {
        matrix_pair_count = 0u;
       // matrix_apply_row_mask(0u);
       // matrix_apply_col_mask(0u);
        return;
    }

    uint8_t prntbuf[80];
    for (uint8_t i = 0u; i < MATRIX_ROW_COUNT; i++)
    {

        sprintf(prntbuf, "apply routine:: row id -> %d, column bits -> %x\r\n", i, *(col_config + i));
        uart1_print(prntbuf);

        (*rowfptr[i])(*(col_config + i));
    }
}

void matrix_disable_mapping(void)
{
    // matrix_apply_pairs(0);
}

int matrix_set_pairs_mapping(const uint8_t *pair_data, uint8_t pair_count)
{
    uint8_t row_idx = 0u;
    uint16_t col_bit_pos = 0u;
    matrix_pair_t parsed_pairs[MATRIX_MAX_PAIRS];
    uint16_t row_container[MATRIX_ROW_COUNT] = {0u};
    uint8_t valid_count = 0u;

    if (pair_count > MATRIX_MAX_PAIRS)
        pair_count = MATRIX_MAX_PAIRS;
    if (pair_count > 0u && pair_data == 0)
        return -1;

    /*
        1. map all colums for each row
           2. Zero out the un mapped column for each row
           3. iterate over each row
            - apply column per row
            ***latching the columns to the row***
            - raise the row high for 10uS
            - low the row
    */

    for (uint8_t i = 0u; i < pair_count; i++)
    {
        uint8_t row = pair_data[(uint16_t)i * 2u];
        uint8_t col = pair_data[(uint16_t)i * 2u + 1u];
        char prntbuf[80];

        if (row < 1u || row > MATRIX_ROW_COUNT || col < 1u || col > MATRIX_COL_COUNT)
            return -1;

        // make the index based on the row
        row_idx = (uint8_t)(row - 1u);
        row_container[row_idx] = row_container[row_idx] | (0x1 << (col - 1));
        sprintf(prntbuf, " row id -> %d, column bits -> %x, col -> %d\r\n", row_idx, row_container[row_idx], col);
        uart1_print(prntbuf);
    }

    matrix_apply_pairs(row_container);
    return 0;
}
