#include "matrix_cfg.h"
#include "matrix_io.h"

/* Row order is V+ (row 1), V- (row 2), I+ (row 3), I- (row 4).
 * Values are matrix columns: DMM[n] is column n+1 and column 9 is GND; 0 means the row is unused. */
static const uint8_t measurement_cfg[MATRIX_MEAS_CFG_COUNT][MATRIX_ROW_COUNT] = {
    {3, 5, 1, 7}, /* config_1 */
    {3, 9, 1, 0}, /* config_2 */
    {7, 9, 5, 0}, /* config_3 */
    {2, 3, 1, 4}, /* config_4 */
    {6, 7, 5, 8}, /* config_5 */
};

static const uint8_t diagnostic_cfg[MATRIX_DIAG_CFG_COUNT][MATRIX_ROW_COUNT] = {
    {1, 2, 1, 2}, /* config_1 */
    {3, 4, 3, 4}, /* config_2 */
    {5, 6, 5, 6}, /* config_3 */
    {7, 8, 7, 8}, /* config_4 */
    {2, 3, 1, 4}, /* config_5 */
    {6, 7, 5, 8}, /* config_6 */
    {3, 5, 1, 7}, /* config_7 */
    {1, 5, 1, 5}, /* config_8 */
};

uint8_t matrix_cfg_count(matrix_mode_t mode)
{
    switch (mode) {
    case MATRIX_MODE_MEASUREMENT:
        return MATRIX_MEAS_CFG_COUNT;
    case MATRIX_MODE_DIAGNOSTIC:
        return MATRIX_DIAG_CFG_COUNT;
    default:
        return 0u;
    }
}

int matrix_apply_config(matrix_mode_t mode, uint8_t config)
{
    const uint8_t *cols;
    uint8_t pair_data[2u * MATRIX_ROW_COUNT];
    uint8_t pair_count = 0u;

    if (config < 1u || config > matrix_cfg_count(mode)) {
        return -1;
    }

    cols = (mode == MATRIX_MODE_MEASUREMENT) ? measurement_cfg[config - 1u]
                                             : diagnostic_cfg[config - 1u];

    for (uint8_t row = 0u; row < MATRIX_ROW_COUNT; row++) {
        if (cols[row] != 0u) {
            pair_data[pair_count * 2u] = (uint8_t)(row + 1u);
            pair_data[pair_count * 2u + 1u] = cols[row];
            pair_count++;
        }
    }

    return matrix_set_pairs_mapping(pair_data, pair_count);
}
