#ifndef MATRIX_IO_H
#define MATRIX_IO_H

#include <stdint.h>

#define MATRIX_ROW_COUNT 4u
#define MATRIX_COL_COUNT 9u

typedef struct {
	uint8_t row;
	uint8_t col;
} matrix_pair_t;

void matrix_init(void);

void matrix_apply_pairs(uint16_t *col_config);


// Compatibility APIs currently used by parser/main.
void matrix_disable_mapping(void);
int matrix_set_pairs_mapping(const uint8_t *pair_data, uint8_t pair_count);


#endif
