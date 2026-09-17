#ifndef FLASH_H
#define FLASH_H

#include "stdint.h"
#include "stm32f1xx.h"



void flash_erase_page(uint32_t pageAddress);
void flash_read32(uint32_t addr, uint32_t *rd32, uint32_t len);
void flash_write32(uint32_t addrs, uint32_t *wr32, uint32_t len);





#endif