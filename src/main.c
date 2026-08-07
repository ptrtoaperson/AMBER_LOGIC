#include "stm32f1xx.h"
#include "spi.h"
#include "lantask.h"
#include "wizchip_conf.h"
#include "w5500.h"
#include "socket.h"
#include <stdio.h>
#include <stdbool.h>
#include "ltc268x.h"
#include "pin_intr.h"
#include "HSE_CLK.h"
#include "parser.h"
#include "uart.h"
#include "I2C.h"
#include "matrix_io.h"
#include "mux_io.h"
#include "flash.h"
#include "fact_reset.h"

volatile bool thereISpkt = false;

static volatile bool uart_cmd_ready = false;
static volatile uint16_t uart_rx_len = 0;
static volatile uint8_t uart_rx_buf[DATA_BUF_SIZE];
static uint8_t uart_cmd_buf[DATA_BUF_SIZE];

// Factory reset trigger: N NRST presses across boots (cleared on power-cycle).
#define FACTORY_RESET_THRESHOLD 3u
#define FACTORY_RESET_CLEAR_DELAY_MS 3000u
#define NETCFG_FLASH_ADDR 0x08007000u
#define NETCFG_MAGIC 0x4E455443u /* 'NETC' */

static void boot_delay_ms(uint32_t ms)
{
    while (ms--) {
        for (volatile uint32_t cycles = 0; cycles < 9000; ++cycles) {
            __asm("nop");
        }
    }
}

static void uart_rx_command_accumulator(char c)
{
    uint8_t byte = (uint8_t)c;

    /* If one full UART frame is waiting, drop incoming bytes until main loop consumes it. */
    if (uart_cmd_ready) {
        return;
    }

    if (uart_rx_len >= (DATA_BUF_SIZE - 1u)) {
        uart_rx_len = 0;
        return;
    }

    uart_rx_buf[uart_rx_len++] = byte;

    if (byte == 'e') {
        uart_cmd_ready = true;
    }
}

static bool is_valid_ipv4_and_port(uint32_t packed_ip, uint32_t port)
{
    uint8_t ip0 = (uint8_t)((packed_ip >> 0) & 0xFFu);
    uint8_t ip1 = (uint8_t)((packed_ip >> 8) & 0xFFu);
    uint8_t ip2 = (uint8_t)((packed_ip >> 16) & 0xFFu);
    uint8_t ip3 = (uint8_t)((packed_ip >> 24) & 0xFFu);

    if (port == 0u || port > 65535u) {
        return false;
    }

    if (ip0 == 0u || ip0 >= 224u) {
        return false;
    }

    if (ip0 == 255u || ip1 == 255u || ip2 == 255u || ip3 == 255u) {
        return false;
    }

    if (ip0 == 127u) {
        return false;
    }

    if ((ip0 | ip1 | ip2 | ip3) == 0u) {
        return false;
    }

    return true;
}

void SystemInit(void) {
    // Force critical startup control pin high as early as possible.
    RCC->APB2ENR |= RCC_APB2ENR_IOPCEN;
    GPIOC->BSRR = GPIO_BSRR_BS0;
    GPIOC->CRL &= ~(GPIO_CRL_CNF0 | GPIO_CRL_MODE0);
    GPIOC->CRL |= GPIO_CRL_MODE0_0; // PC0 output push-pull, 10MHz

    // Earliest possible DAC safety state on reset.
    ltc_spi_init();
    ltc_write_dac_cs(LTC268X_CMD_POWERDOWN_REG, 0xFFFF, LTC_DAC_CS0);
}

int main(void)
{
    X_Clock_Init();

    // Bring DAC SPI/CS up immediately, then force DAC into power-down.
    // so outputs do not sit at POR defaults during early boot delays.
    ltc_spi_init();
    ltc_write_dac_cs(LTC268X_CMD_POWERDOWN_REG, 0xFFFF, LTC_DAC_CS0);

    uart1_init();
    uart1_set_rx_callback(uart_rx_command_accumulator);

    uart1_print("Clock test\r\n");

    ExternIntInit();
    ethernet_spi_init();
    matrix_init();
    mux_init();

    __enable_irq(); // Global interrupt enable

    hardware_reset();

    w5500_port_init();

    uart1_print("SWD test\r\n");

    // Keep channels powered down while loading startup configuration.
    ltc_write_dac_cs(LTC268X_CMD_POWERDOWN_REG, 0xFFFF, LTC_DAC_CS0);

    // Initialize LTC2688 channels to +/-10V span and 0V code.
    // Channel settings are buffered, so issue an explicit channel update
    // after writing both setting and code registers.
    for (uint8_t ch = 0; ch < 16; ch++) {
        uint8_t reg_setting = LTC268X_CMD_CH_SETTING(ch, LTC2688);
        uint8_t reg_code = LTC268X_CMD_CH_CODE(ch, LTC2688);
        uint8_t reg_update = LTC268X_CMD_CH_UPDATE(ch, LTC2688);
        ltc_write_dac_cs(reg_setting, LTC268X_CH_SPAN(LTC268X_VOLTAGE_RANGE_M10V_10V), LTC_DAC_CS0);
        ltc_write_dac_cs(reg_code, 0x8000, LTC_DAC_CS0);
        ltc_write_dac_cs(reg_update, 0x0000, LTC_DAC_CS0);
    }

    // Power up only after safe code/span preload is complete.
    ltc_write_dac_cs(LTC268X_CMD_POWERDOWN_REG, 0x0000, LTC_DAC_CS0);
    uart1_print("LTC2688 initialized (channels 0-15).\r\n");

    // ========================================================================
    // 2. PERIPHERAL GPIO & W5500 ETHERNET SERVER SETUP
    // ========================================================================
    // Enable the clock for GPIOC
    RCC->APB2ENR |= RCC_APB2ENR_IOPCEN;

    // Configure PC13 as General Purpose Output Push-Pull (2MHz)
    GPIOC->CRH &= ~(GPIO_CRH_MODE13 | GPIO_CRH_CNF13); 
    GPIOC->CRH |= GPIO_CRH_MODE13_1;

    // Memory Allocation: 2KB for all 8 sockets
    uint8_t memsize[2][8] = {{2, 2, 2, 2, 2, 2, 2, 2}, {2, 2, 2, 2, 2, 2, 2, 2}};
    (void)memsize; // FIX: Silences unused variable warning cleanly

    // Enable specific interrupt types (RECV for data, CON for connection) grouped safely
    setSn_IMR(0, (Sn_IR_RECV | Sn_IR_CON));

    wiz_NetInfo netinfo = {
        .mac = {0x00, 0x08, 0xDC, 0x11, 0x22, 0x33},
        .ip = {192, 168, 1, 50},
        .sn = {255, 255, 255, 0},
        .gw = {192, 168, 1, 1},
        .dns = {8, 8, 8, 8},
        .dhcp = NETINFO_STATIC};

    i2c_init();
    if (eui48_read_mac(netinfo.mac)) {
        char mac_msg[64];
        snprintf(mac_msg,
                 sizeof(mac_msg),
                 "MAC from EEPROM: %02X:%02X:%02X:%02X:%02X:%02X\r\n",
                 netinfo.mac[0],
                 netinfo.mac[1],
                 netinfo.mac[2],
                 netinfo.mac[3],
                 netinfo.mac[4],
                 netinfo.mac[5]);
        uart1_print(mac_msg);
    } else {
        uart1_print("EEPROM MAC read failed, using fallback MAC.\r\n");
    }

    // Factory reset logic: rapid resets erase persisted network config page.
    uint16_t rstCnt = Increment_Reset_Counter(0u);
    char dispbuf[64];
    snprintf(dispbuf,
             sizeof(dispbuf),
             "rst count = %u (trigger=%u)\r\n",
             (unsigned int)rstCnt,
             (unsigned int)FACTORY_RESET_THRESHOLD);
    uart1_print(dispbuf);

    if (rstCnt >= FACTORY_RESET_THRESHOLD) {
        flash_erase_page(NETCFG_FLASH_ADDR);
        uart1_print("Factory reset: flash page erased\r\n");
        Increment_Reset_Counter(1u);
    } else {
        // If no further quick resets happen in this window, clear the counter.
        boot_delay_ms(FACTORY_RESET_CLEAR_DELAY_MS);
        Increment_Reset_Counter(1u);
    }

    // Load persisted IP and TCP port from flash when valid.
    uint32_t flash_data[3] = {0u, 0u, 0u};
    flash_read32(NETCFG_FLASH_ADDR, flash_data, 3u);

    char rawcfg[128];
    snprintf(rawcfg,
             sizeof(rawcfg),
             "Flash raw cfg: IP_WORD=0x%08lX PORT_WORD=0x%08lX MAGIC=0x%08lX\r\n",
             (unsigned long)flash_data[0],
             (unsigned long)flash_data[1],
             (unsigned long)flash_data[2]);
    uart1_print(rawcfg);

    bool netcfg_valid = (flash_data[2] == NETCFG_MAGIC)
                        && is_valid_ipv4_and_port(flash_data[0], flash_data[1]);

    if (netcfg_valid) {
        netinfo.ip[0] = (uint8_t)((flash_data[0] >> 0) & 0xFFu);
        netinfo.ip[1] = (uint8_t)((flash_data[0] >> 8) & 0xFFu);
        netinfo.ip[2] = (uint8_t)((flash_data[0] >> 16) & 0xFFu);
        netinfo.ip[3] = (uint8_t)((flash_data[0] >> 24) & 0xFFu);

        char netbuf[96];
        snprintf(netbuf,
                 sizeof(netbuf),
                 "Flash network config: %u.%u.%u.%u\r\n",
                 netinfo.ip[0],
                 netinfo.ip[1],
                 netinfo.ip[2],
                 netinfo.ip[3]);
        uart1_print(netbuf);
    } else {
        uart1_print("Flash network config invalid/missing; using defaults\r\n");
    }

    wizchip_setnetinfo(&netinfo);

    uint16_t server_port = TCP_PORT;
    if (netcfg_valid) {
        server_port = (uint16_t)flash_data[1];
    }

    set_tcp_server_port(server_port);
    socket(0, Sn_MR_TCP, server_port, 0);
    listen(0);

    setSIMR(0x01);

    // Initial Hardware Check
    uint8_t ver = getVERSIONR();
    if (ver != 0x04)
    {
        uart1_print("FATAL ERROR: W5500 not found via SPI!\r\n");
        while (1) {
            // Rapidly flash the LED if there is a hardware error
            GPIOC->BRR = (1 << 13);  // LED ON
            for (volatile int i = 0; i < 300000; i++);
            GPIOC->BSRR = (1 << 13); // LED OFF
            for (volatile int i = 0; i < 300000; i++);
        }
    }
    uart1_print("W5500 Ready. Starting Server...\r\n");

    // ========================================================================
    // 3. MAIN BACKGROUND SERVER LOOP
    // ========================================================================
    while (1) {
        if (uart_cmd_ready) {
            uint16_t frame_len = 0;

            __disable_irq();
            frame_len = uart_rx_len;
            if (frame_len > DATA_BUF_SIZE) {
                frame_len = DATA_BUF_SIZE;
            }
            for (uint16_t i = 0; i < frame_len; i++) {
                uart_cmd_buf[i] = uart_rx_buf[i];
            }
            uart_rx_len = 0;
            uart_cmd_ready = false;
            __enable_irq();

            if (frame_len > 0) {
                uart1_print("UART packet processing...\r\n");
                uint16_t parse_len = frame_len;
                if (parse_len > 0 && uart_cmd_buf[parse_len - 1] == 'e') {
                    parse_len--;
                }
                parse_command(uart_cmd_buf, parse_len);
            }
        }

        // Check the flag OR check if the pin is physically LOW (PD2)
        if (thereISpkt || !(GPIOD->IDR & (1 << 2))) {
            thereISpkt = true; // Force flag true if pin is held low by W5500
            uart1_print("Packet processing...\r\n");
            
            run_tcp_server(&thereISpkt);
            
            // Clear the pending register for line 2 
            // to catch any edge that happened during processing
            EXTI->PR = EXTI_PR_PR2; 
        }
    }
}