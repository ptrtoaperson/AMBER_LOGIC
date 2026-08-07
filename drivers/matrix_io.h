#ifndef MATRIX_IO_H
#define MATRIX_IO_H

#include <stdint.h>

#define MATRIX_ROW_COUNT 4u
#define MATRIX_COL_COUNT 9u

void matrix_init(void);
void matrix_apply_masks(uint16_t row_mask, uint16_t col_mask);

#endif
