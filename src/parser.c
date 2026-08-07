#include "parser.h"
#include "ltc268x.h"
#include "matrix_io.h"
#include "mux_io.h"
#include "flash.h"
#include <stdio.h>
#include "uart.h"

#define NETCFG_FLASH_ADDR 0x08007000u
#define NETCFG_MAGIC 0x4E455443u /* 'NETC' */

typedef struct {
    uint8_t local_ch;
    enum ltc268x_device_id dev_id;
    ltc_dac_cs_t cs;
} dac_route_t;

static int map_logical_channel(uint8_t logical_ch, dac_route_t *route)
{
    if (!route) {
        return -1;
    }

    if (logical_ch < 16u) {
        route->local_ch = logical_ch;
        route->dev_id = LTC2688;
        route->cs = LTC_DAC_CS0;
        return 0;
    }

    return -1;
}

uint8_t parse_command(uint8_t *dptr, uint16_t len)
{
    uint16_t pos = 0;

    while (pos < len) {
        uint8_t inst = dptr[pos];

        if (inst == (uint8_t)'e') {
            break;
        }

        if (inst == 1u) {
            if ((uint16_t)(len - pos) < sizeof(Vcommand_t)) {
                break;
            }
            pos = (uint16_t)(pos + (uint16_t)set_voltage((char *)&dptr[pos]));
            continue;
        }

        if (inst == 8u) {
            if ((uint16_t)(len - pos) < MATRIX_CMD_LEN) {
                break;
            }
            pos = (uint16_t)(pos + (uint16_t)set_matrix((char *)&dptr[pos]));
            continue;
        }

        if (inst == 9u) {
            if ((uint16_t)(len - pos) < MUX_CMD_LEN) {
                break;
            }
            pos = (uint16_t)(pos + (uint16_t)set_mux((char *)&dptr[pos]));
            continue;
        }

        if (inst == 10u) {
            if ((uint16_t)(len - pos) < WRITE_IP_CMD_LEN) {
                break;
            }
            pos = (uint16_t)(pos + (uint16_t)writeIP((char *)&dptr[pos]));
            continue;
        }

        // Unsupported instruction in DAC+matrix+mux profile.
        break;
    }

    return 1u;
}

int set_voltage(char *args)
{
    Vcommand_t *cmd = (Vcommand_t *)args;
    dac_route_t route;
    char prntbuf[60];
    snprintf(prntbuf, sizeof(prntbuf), "reached set_voltage: N_DAC=%u, CODE=0x%04X\r\n", cmd->N_DAC, cmd->CODE);
    uart1_print(prntbuf);
    
    if (map_logical_channel(cmd->N_DAC, &route) < 0) {
        return (int)sizeof(*cmd);
    }

    ltc_write_dac_cs(LTC268X_CMD_CH_CODE_UPDATE(route.local_ch, route.dev_id), cmd->CODE, route.cs);
    return (int)sizeof(*cmd);
}

int set_matrix(char *args)
{
    MATRIXcommand_t *cmd = (MATRIXcommand_t *)args;
    matrix_apply_masks(cmd->ROW_MASK, cmd->COL_MASK);

    char prntbuf[60];
    snprintf(prntbuf, sizeof(prntbuf), "reached set_matrix: row_mask=0x%04X, col_mask=0x%04X\r\n", cmd->ROW_MASK, cmd->COL_MASK);
    uart1_print(prntbuf);
    return MATRIX_CMD_LEN;
}

int set_mux(char *args)
{
    MUXcommand_t *cmd = (MUXcommand_t *)args;
    mux_select(cmd->MUX_ID);
    char prntbuf[60];
    snprintf(prntbuf, sizeof(prntbuf), "reached set_mux: mux_id=%u\r\n", cmd->MUX_ID);
    uart1_print(prntbuf);
    return MUX_CMD_LEN;
}

int writeIP(char *args)
{
    WriteIPcommand_t *cmd = (WriteIPcommand_t *)args;
    char prntbuf[160];

    uint32_t flash_data_write[3];
    flash_data_write[0] =
        ((uint32_t)cmd->IPaddr[0] << 0)
        | ((uint32_t)cmd->IPaddr[1] << 8)
        | ((uint32_t)cmd->IPaddr[2] << 16)
        | ((uint32_t)cmd->IPaddr[3] << 24);
    flash_data_write[1] = (uint32_t)cmd->PORT;
    flash_data_write[2] = NETCFG_MAGIC;

    flash_write32(NETCFG_FLASH_ADDR, flash_data_write, 3u);

    uint32_t flash_verify[3] = {0u, 0u, 0u};
    flash_read32(NETCFG_FLASH_ADDR, flash_verify, 3u);

    if (flash_verify[0] != flash_data_write[0]
        || flash_verify[1] != flash_data_write[1]
        || flash_verify[2] != flash_data_write[2]) {
        snprintf(prntbuf,
                 sizeof(prntbuf),
                 "Flash verify FAILED. wrote=[0x%08lX 0x%08lX 0x%08lX] read=[0x%08lX 0x%08lX 0x%08lX]\r\n",
                 (unsigned long)flash_data_write[0],
                 (unsigned long)flash_data_write[1],
                 (unsigned long)flash_data_write[2],
                 (unsigned long)flash_verify[0],
                 (unsigned long)flash_verify[1],
                 (unsigned long)flash_verify[2]);
        uart1_print(prntbuf);
    }

    snprintf(prntbuf,
             sizeof(prntbuf),
             "Stored IP %u.%u.%u.%u, port %u to flash\r\n",
             cmd->IPaddr[0],
             cmd->IPaddr[1],
             cmd->IPaddr[2],
             cmd->IPaddr[3],
             (unsigned int)cmd->PORT);
    uart1_print(prntbuf);

    return WRITE_IP_CMD_LEN;
}
