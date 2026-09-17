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

        if (inst == MATRIX_PAIRS_INST) {
            uint16_t remaining = (uint16_t)(len - pos);
            int consumed;

            if (remaining < MATRIX_PAIRS_HDR_LEN) {
                break;
            }

            consumed = set_matrix_pairs((char *)&dptr[pos], remaining);
            if (consumed <= 0) {
                break;
            }

            pos = (uint16_t)(pos + (uint16_t)consumed);
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

int set_matrix_pairs(char *args, uint16_t avail_len)
{
    MATRIXpairsHdr_t *hdr = (MATRIXpairsHdr_t *)args;
    uint16_t needed_len;
    uint8_t flags = hdr->FLAGS;
    uint8_t pair_count = hdr->PAIR_COUNT;
    uint8_t *pair_data = (uint8_t *)args + MATRIX_PAIRS_HDR_LEN;
    char prntbuf[80];

    needed_len = (uint16_t)(MATRIX_PAIRS_HDR_LEN + ((uint16_t)pair_count * 2u));
    if (avail_len < needed_len) {
        return -1;
    }
//to be checked ?????
/*
    if ((flags & (MATRIX_PAIRS_FLAG_CLEAR_BETWEEN | MATRIX_PAIRS_FLAG_CLEAR_AT_END)) != 0u) {
        uint8_t idx;
        matrix_disable_mapping();
        for (idx = 0u; idx < pair_count; idx++) {
            uint8_t row = pair_data[(uint16_t)idx * 2u];
            uint8_t col = pair_data[(uint16_t)idx * 2u + 1u];


            if (row < 1u || row > MATRIX_ROW_COUNT || col < 1u || col > MATRIX_COL_COUNT) {
                continue;
            }

            matrix_apply_masks((uint16_t)(1u << (row - 1u)), (uint16_t)(1u << (col - 1u)));

            if ((flags & MATRIX_PAIRS_FLAG_CLEAR_BETWEEN) != 0u && idx + 1u < pair_count) {
                matrix_apply_masks(0u, 0u);
            }
        }

        if ((flags & MATRIX_PAIRS_FLAG_CLEAR_AT_END) != 0u) {
            matrix_apply_masks(0u, 0u);
        }

        return (int)needed_len;
    }
        */

    if (matrix_set_pairs_mapping(pair_data, pair_count) < 0) {
        snprintf(prntbuf,
                 sizeof(prntbuf),
                 "matrix pair map rejected: count=%u\r\n",
                 (unsigned int)pair_count);
        uart1_print(prntbuf);
        matrix_disable_mapping();
    } else {
        snprintf(prntbuf,
                 sizeof(prntbuf),
                 "matrix pair map set: count=%u\r\n",
                 (unsigned int)pair_count);
        uart1_print(prntbuf);
    }

    return (int)needed_len;
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
