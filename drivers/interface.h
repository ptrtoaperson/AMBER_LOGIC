#ifndef INTERFACE_H
#define INTERFACE_H

#include <stdint.h>
#include "parser.h"
#include "matrix_io.h"
#include "mux_io.h"
#include "ltc268x.h"

#define DAC_SIZE    14U
#define ADDR_SIZE   10U

#define ADDR_1      0U
#define ADDR_2      1U
#define ADDR_3      2U
#define ADDR_4      3U
#define ADDR_5      4U
#define ADDR_6      5U
#define ADDR_7      6U
#define ADDR_8      7U
#define ADDR_9      8U
#define ADDR_10     9U
#define EN_LT_POS   10U
#define VCC_POS     11U
#define SUB_POS     12U
#define VSS_POS     13U



/* Logic-level DAC codes for a +/-10V bipolar span: code = ((volts+10)/20)*65536.
 * Runtime-configurable via the "configlevels" shell command and persisted to flash. */
extern uint16_t VCC;
extern uint16_t VSS;
extern uint16_t SUB;
extern uint16_t BIT_HIGH;
extern uint16_t BIT_LOW;
extern uint16_t EN_LT;

void interface_init(void);
int set_power(uint8_t sw);
int set_address(uint16_t address);
int set_enable_lt(uint8_t en);
void interface_reapply(void);

#endif // INTERFACE_H   