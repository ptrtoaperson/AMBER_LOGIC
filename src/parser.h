#ifndef PARSER_H
#define PARSER_H

#include "stm32f1xx.h"
#include <stdint.h>
#include "ltc268x.h"
#include "matrix_io.h"
#include "mux_io.h"
#include "spi.h"
#include "uart.h"

#define MATRIX_CMD_LEN 5u
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
    uint16_t ROW_MASK;
    uint16_t COL_MASK;
} MATRIXcommand_t;

typedef struct __attribute__((packed)) {
    uint8_t INST;
    uint8_t MUX_ID;
} MUXcommand_t;

typedef struct __attribute__((packed)) {
    uint8_t INST;
    uint8_t IPaddr[4];
    uint16_t PORT;
} WriteIPcommand_t;

uint8_t parse_command(uint8_t *dptr, uint16_t len);
int set_voltage(char *args);
int set_matrix(char *args);
int set_mux(char *args);
int writeIP(char *args);

#endif
