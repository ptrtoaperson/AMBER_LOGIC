
#ifndef _MATRIX_CONFIG_H
#define _MATRIX_CONFIG_H

#include <stdint.h>

// LSB is DMM0 and MSB is GND
// measurement config modes 
const uint16_t R_1_MEAS_CFG[5] = {0x04, 0x04, 0x40, 0x02, 0x20 }; //V+ or A+  	 
const uint16_t  R_2_MEAS_CFG[5] = {0x10, 0x100, 0x100, 0x10, 0x20}; //V- or A-
const uint16_t R_3_MEAS_CFG[5] = {0x01, 0x01, 0x10, 0x01, 0x10}; //I+ or B+
const uint16_t R_4_MEAS_CFG[5] = {0x40, 0x00, 0x00, 0x08, 0x80}; //I- or B-
								 //
//Diagnostic config modes

const uint16_t R_1_DIAG_CFG[8] = {}



#endif 
