#include "interface.h"
#include "ltc268x.h"
#include "spi.h"




uint16_t addr_register[DAC_SIZE];

uint16_t VCC      = 40960U;
uint16_t VSS      = 24576U;
uint16_t SUB      = 32768U;
uint16_t BIT_HIGH = 40960U;
uint16_t BIT_LOW  = 24576U;
uint16_t EN_LT    = 40960U;

/* Last requested address/enable/power, so levels can be re-applied after they change. */
static uint16_t s_address = 0u;
static uint8_t s_enable = 0u;
static uint8_t s_power = 0u;
static uint8_t s_loaded = 0u; /* bit0: address, bit1: enable, bit2: power */

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
    }

//        ltc_write_dac_cs(LTC268X_CMD_CH_CODE(i, LTC2688), addr_register[i], LTC_DAC_CS0);
  //  }
    //ltc_write_dac_cs(LTC268X_CMD_UPDATE_ALL, 0x0000, LTC_DAC_CS0);


}

static void load_address(uint16_t address)
{
    for (uint8_t i = 0; i < ADDR_SIZE; i++) {
        addr_register[i] = address & ((uint16_t)1 << i) ? BIT_HIGH : BIT_LOW;
    }
}

static void load_enable_lt(uint8_t en)
{
    addr_register[EN_LT_POS] = en ? EN_LT : SUB;
}

static void load_power(uint8_t sw)
{
    addr_register[VCC_POS] = sw ? VCC : SUB;
    addr_register[SUB_POS] = SUB;
    addr_register[VSS_POS] = VSS;
}

int set_address(uint16_t address){
    s_address = address;
    s_loaded |= 1u;
    load_address(address);
    apply_addr_register();
    return 0;
}

int set_enable_lt(uint8_t en){
    s_enable = en;
    s_loaded |= 2u;
    load_enable_lt(en);
    apply_addr_register();
    return 0;
}

int set_power(uint8_t sw){
    s_power = sw;
    s_loaded |= 4u;
    load_power(sw);
    apply_addr_register();
    return 0;
}

/* Loads address, enable and power into addr_register[] and writes all channels once. */
int set_address_enable_power(uint16_t address, uint8_t en, uint8_t sw)
{
    s_address = address;
    s_enable = en;
    s_power = sw;
    s_loaded |= 7u;
    load_address(address);
    load_enable_lt(en);
    load_power(sw);
    apply_addr_register();
    return 0;
}

void interface_reapply(void)
{
    /* Nothing driven yet: just refresh idle codes, don't touch the DAC outputs. */
    if (s_loaded == 0u) {
        interface_init();
        return;
    }

    if (s_loaded & 1u) {
        load_address(s_address);
    }
    if (s_loaded & 2u) {
        load_enable_lt(s_enable);
    }
    if (s_loaded & 4u) {
        load_power(s_power);
    }
    apply_addr_register();
}
