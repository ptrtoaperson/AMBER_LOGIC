#include "flash.h"





void flash_read32(uint32_t addr, uint32_t *rd32, uint32_t len)
{
  //Read data can be accessed straight away
  uint32_t tempAddrs = addr;

  for(uint32_t i=0; i<len; i++)
  {
    rd32[i] = *(__IO uint32_t*) tempAddrs;
    tempAddrs+=4;
  }
}



// Write flash page safely (STM32F1)
void flash_erase_page(uint32_t pageAddress)
{
    // Unlock if locked
    if (FLASH->CR & FLASH_CR_LOCK)
    {
        FLASH->KEYR = 0x45670123;
        FLASH->KEYR = 0xCDEF89AB;
    }

    while (FLASH->SR & FLASH_SR_BSY);

    // Clear stale completion/error flags before starting a new erase.
    FLASH->SR |= (FLASH_SR_EOP | FLASH_SR_PGERR | FLASH_SR_WRPRTERR);

    // Page erase
    FLASH->CR |= FLASH_CR_PER;
    FLASH->AR = pageAddress;
    FLASH->CR |= FLASH_CR_STRT;

    while (FLASH->SR & FLASH_SR_BSY);

    // Acknowledge completion/error flags for the next operation.
    FLASH->SR |= (FLASH_SR_EOP | FLASH_SR_PGERR | FLASH_SR_WRPRTERR);

    FLASH->CR &= ~FLASH_CR_PER;

    // Lock optional here if you want
}



// Write 32-bit array to flash safely
void flash_write32(uint32_t address, uint32_t *data, uint32_t len)
{
    uint32_t tempAddr = address;

    // Unlock flash
    if (FLASH->CR & FLASH_CR_LOCK)
    {
        FLASH->KEYR = 0x45670123;
        FLASH->KEYR = 0xCDEF89AB;
    }

    // Erase the page once before writing
    flash_erase_page(address);

    // Clear stale completion/error flags before programming sequence.
    FLASH->SR |= (FLASH_SR_EOP | FLASH_SR_PGERR | FLASH_SR_WRPRTERR);

    for (uint32_t i = 0; i < len; i++)
    {
        uint16_t low  = (uint16_t)(data[i] & 0xFFFF);
        uint16_t high = (uint16_t)((data[i] >> 16) & 0xFFFF);

        // --- Write lower half-word ---
        FLASH->CR |= FLASH_CR_PG;
        *(__IO uint16_t*)tempAddr = low;
        while (FLASH->SR & FLASH_SR_BSY);
        // Check EOP
        if (FLASH->SR & FLASH_SR_EOP)
            FLASH->SR |= FLASH_SR_EOP;
        FLASH->CR &= ~FLASH_CR_PG;

        // Optional: check errors and abort write sequence.
        if ((FLASH->SR & FLASH_SR_PGERR) || (FLASH->SR & FLASH_SR_WRPRTERR)) {
            break;
        }

        tempAddr += 2;

        // --- Write upper half-word ---
        FLASH->CR |= FLASH_CR_PG;
        *(__IO uint16_t*)tempAddr = high;
        while (FLASH->SR & FLASH_SR_BSY);
        if (FLASH->SR & FLASH_SR_EOP)
            FLASH->SR |= FLASH_SR_EOP;
        FLASH->CR &= ~FLASH_CR_PG;

        if ((FLASH->SR & FLASH_SR_PGERR) || (FLASH->SR & FLASH_SR_WRPRTERR)) {
            break;
        }

        tempAddr += 2;
    }

    // Lock flash
    FLASH->CR |= FLASH_CR_LOCK;


}
