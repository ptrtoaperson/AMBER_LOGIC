#ifndef MATRIX_CFG_H
#define MATRIX_CFG_H

#include <stdint.h>

typedef enum {
    MATRIX_MODE_MEASUREMENT = 0,
    MATRIX_MODE_DIAGNOSTIC = 1
} matrix_mode_t;

#define MATRIX_MEAS_CFG_COUNT 5u
#define MATRIX_DIAG_CFG_COUNT 8u

/* Number of configs available for the mode, or 0 if the mode is invalid. */
uint8_t matrix_cfg_count(matrix_mode_t mode);

/* Applies the named config (1-based, i.e. "config_1" -> 1) of the given mode to the matrix.
 * Returns 0 on success, -1 if the mode/config is out of range or the mapping is rejected. */
int matrix_apply_config(matrix_mode_t mode, uint8_t config);

#endif
