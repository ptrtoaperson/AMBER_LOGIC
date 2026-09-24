#include "interface.h"
#include "ltc268x.h"
#include "spi.h"

/* Defaults match the previous hardcoded macro values (2.5V/-2.5V/0V). */
uint16_t VCC      = 40960U;
uint16_t VSS      = 24576U;
uint16_t SUB      = 32768U;
uint16_t BIT_HIGH = 40960U;
uint16_t BIT_LOW  = 24576U;
uint16_t EN_LT    = 40960U;

uint16_t addr_register[DAC_SIZE];

/* Last commanded logical state, so interface_reapply() can re-derive addr_register
 * from the current BIT_HIGH/BIT_LOW/EN_LT/VCC/VSS/SUB codes after they change. */
static uint16_t s_last_address = 0u;
static uint8_t s_last_enable = 0u;
static uint8_t s_last_power = 0u;

/* addr_register[] must default to BIT_LOW (0V), not raw code 0 (-10V), so
 * unset channels idle at logic-0 before set_address/set_enable_lt/set_power
 * are ever called. */
void interface_init(void)
{
    for (uint8_t i = 0; i < DAC_SIZE; i++) {
        addr_register[i] = BIT_LOW;
    }
}

/* Loads every channel's code register from addr_register, then latches them to the outputs together
 * via the standalone system UPDATE_ALL command (0x7C) with a dummy payload -- this is the exact
 * pattern the ADI reference driver uses (see ltc268x.c: ltc268x_init()) after configuring channels. */
static void apply_addr_register(void)
{

     
    for (uint8_t i = 0; i < DAC_SIZE; i++) {
   write_dac_voltage(i, addr_register[i]);
    // small delay to ensure the DAC has time to process the command
    for (volatile int j = 0; j < 72000; j++);

    }

//        ltc_write_dac_cs(LTC268X_CMD_CH_CODE(i, LTC2688), addr_register[i], LTC_DAC_CS0);
  //  }
    //ltc_write_dac_cs(LTC268X_CMD_UPDATE_ALL, 0x0000, LTC_DAC_CS0);


}

int set_address(uint16_t address){

s_last_address = address;

for(uint8_t i = 0; i<ADDR_SIZE; i++){
    addr_register[i] = address & ((uint16_t)1 << i) ? BIT_HIGH : BIT_LOW ;
}

apply_addr_register();

return 0;
}

int set_enable_lt(uint8_t en){
    s_last_enable = en;
    addr_register[EN_LT_POS] = en ? EN_LT:SUB;
    apply_addr_register();
    return 0;
}

int set_power(uint8_t sw){

    s_last_power = sw;
    addr_register[VCC_POS] = sw ? VCC : SUB;
    addr_register[SUB_POS] = sw ? SUB : SUB;
    addr_register[VSS_POS] = VSS;

    apply_addr_register();
return 0;    
}

/* Re-latches the last commanded address/enable/power state using the current
 * BIT_HIGH/BIT_LOW/EN_LT/VCC/VSS/SUB codes -- call after those change at runtime. */
void interface_reapply(void)
{
    set_address(s_last_address);
    set_enable_lt(s_last_enable);
    set_power(s_last_power);
}