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



/* Codes assume a +/-10V bipolar DAC span: 0V=0x8000 (midscale), 5V=0xC000. */
#define VCC          40960U//49152U//5.2f   // convert to code 49152
#define VSS          24576U//32768U//0.0f   //32768
#define SUB          32768U//0.0f   //32768

#define BIT_HIGH    40960U//49152U//5.0f    //49152
#define BIT_LOW     24576U//32768U//0.0f    //32768

#define EN_LT       40960U//49152U//5.0f    //49152


void interface_init(void);
int set_power(uint8_t sw);
int set_address(uint16_t address);
int set_enable_lt(uint8_t en);

#endif // INTERFACE_H   