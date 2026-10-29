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
#define VSS_POS     12U
#define SUB_POS     13U



/* Levels are DAC codes for a +/-10V bipolar span: 0V=0x8000 (midscale), 5V=0xC000.
 * Defaults are defined in interface.c and can be overridden from flash via configlevels. */
extern uint16_t VCC;
extern uint16_t VSS;
extern uint16_t SUB;
extern uint16_t BIT_HIGH;
extern uint16_t BIT_LOW;
extern uint16_t EN_LT;

void interface_init(void);
/* Recomputes the channel codes from the current levels and writes them to the DAC. */
void interface_reapply(void);
int set_power(uint8_t sw);
int set_address(uint16_t address);
int set_enable_lt(uint8_t en);
int set_address_enable_power(uint16_t address, uint8_t en, uint8_t sw);

#endif // INTERFACE_H   