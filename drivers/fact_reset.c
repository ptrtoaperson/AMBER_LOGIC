#include "fact_reset.h"

uint16_t Increment_Reset_Counter(uint8_t clr) {
    // Enable Clock for PWR and BKP peripherals
    RCC->APB1ENR |= (RCC_APB1ENR_PWREN | RCC_APB1ENR_BKPEN);

    // Disable Backup Domain write protection (Unlock)
    PWR->CR |= PWR_CR_DBP;

    // Read, Increment, and Write back to Backup Data Register 1
    // STM32F103 has 10/42 16-bit backup registers (DR1 to DR42)


    uint16_t count = BKP->DR1;
    uint32_t rst_flags = RCC->CSR;

    // Treat as true power-on reset only when POR is set without other reset causes.
    const uint32_t non_por_mask =
        (RCC_CSR_PINRSTF | RCC_CSR_SFTRSTF | RCC_CSR_IWDGRSTF | RCC_CSR_WWDGRSTF | RCC_CSR_LPWRRSTF);
    const uint8_t is_power_on_only =
        ((rst_flags & RCC_CSR_PORRSTF) != 0u) && ((rst_flags & non_por_mask) == 0u);

    if (is_power_on_only) {
        count = 0u;
        BKP->DR1 = 0u;
    } else if (clr == 1u) {
        count = 0u;
        BKP->DR1 = 0u;
    } else {
        count = (uint16_t)(count + 1u);
        BKP->DR1 = count;
    }
    // (Optional) Re-enable protection
    PWR->CR &= ~PWR_CR_DBP;

    // Clear Reset Flags in RCC_CSR
    // This allows you to detect the NEXT reset type accurately
    RCC->CSR |= RCC_CSR_RMVF;
    return count;
}
// It requires timer to clear the the reset flag !!!!!!!!!!!!!!!!!!!!!!!!