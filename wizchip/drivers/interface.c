#include "interface.h"
#include "ltc268x.h"
#include "spi.h"




uint16_t addr_register[DAC_SIZE]= {0};

/* Loads every channel's code register from addr_register, then latches them to the outputs together
 * via the standalone system UPDATE_ALL command (0x7C) with a dummy payload -- this is the exact
 * pattern the ADI reference driver uses (see ltc268x.c: ltc268x_init()) after configuring channels. */
static void apply_addr_register(void)
{
    for (uint8_t i = 0; i < DAC_SIZE; i++) {
        ltc_write_dac_cs(LTC268X_CMD_CH_CODE(i, LTC2688), addr_register[i], LTC_DAC_CS0);
    }
    ltc_write_dac_cs(LTC268X_CMD_UPDATE_ALL, 0x0000, LTC_DAC_CS0);
}

int set_address(uint16_t address){
   
for(uint8_t i = 0; i<ADDR_SIZE; i++){
    addr_register[i] = address & ((uint16_t)1 << i) ? BIT_HIGH : BIT_LOW ;
}

apply_addr_register();

return 0;
}

int set_enable_lt(uint8_t en){
    addr_register[EN_LT_POS] = en ? EN_LT:BIT_LOW;
    apply_addr_register();
    return 0;
}

int set_power(uint8_t sw){

    addr_register[VCC_POS] = sw ? VCC : VSS;
    addr_register[SUB_POS] = sw ? SUB : VSS;
    addr_register[VSS_POS] = VSS;

    apply_addr_register();
return 0;    
}