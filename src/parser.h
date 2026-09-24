#ifndef PARSER_H
#define PARSER_H

#include "stm32f1xx.h"
#include <stdint.h>
#include "ltc268x.h"
#include "matrix_io.h"
#include "mux_io.h"
#include "spi.h"
#include "uart.h"

#define MATRIX_PAIRS_INST 11u

#define MATRIX_PAIRS_FLAG_CLEAR_BETWEEN 0x01u
#define MATRIX_PAIRS_FLAG_CLEAR_AT_END 0x02u

#define MATRIX_PAIRS_HDR_LEN 3u
#define MUX_CMD_LEN 2u
#define WRITE_IP_CMD_LEN 7u

typedef struct __attribute__((packed))
{
    uint8_t INST;
    uint8_t N_DAC;
    uint16_t CODE;
} Vcommand_t;

typedef struct __attribute__((packed)) {
    uint8_t INST;
    uint8_t FLAGS;
    uint8_t PAIR_COUNT;
} MATRIXpairsHdr_t;

typedef struct __attribute__((packed)) {
    uint8_t INST;
    uint8_t MUX_ID;
} MUXcommand_t;

typedef struct __attribute__((packed)) {
    uint8_t INST;
    uint8_t IPaddr[4];
    uint16_t PORT;
} WriteIPcommand_t;

typedef struct {
    uint8_t local_ch;
    enum ltc268x_device_id dev_id;
    ltc_dac_cs_t cs;
} dac_route_t;

int map_logical_channel(uint8_t logical_ch, dac_route_t *route);
int write_dac_voltage(uint8_t logical_ch, uint16_t code);
uint8_t parse_command(uint8_t *dptr, uint16_t len);
int set_voltage(char *args);
int set_matrix_pairs(char *args, uint16_t avail_len);
int set_mux(char *args);
int writeIP(char *args);

#endif
