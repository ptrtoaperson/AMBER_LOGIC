#include "lantask.h"
#include "parser.h"
#include "rest_parser.h"
#include <stdio.h>

static const char *socket_state_name(uint8_t state)
{
    switch (state)
    {
    case SOCK_CLOSED:
        return "SOCK_CLOSED";
    case SOCK_INIT:
        return "SOCK_INIT";
    case SOCK_LISTEN:
        return "SOCK_LISTEN";
    case SOCK_ESTABLISHED:
        return "SOCK_ESTABLISHED";
    case SOCK_CLOSE_WAIT:
        return "SOCK_CLOSE_WAIT";
    default:
        return "SOCK_OTHER";
    }
}

uint8_t g_dat_buf[DATA_BUF_SIZE];
uint8_t sn = 0;
static uint16_t g_server_port = TCP_PORT;

void set_tcp_server_port(uint16_t port)
{
    if (port == 0u)
    {
        g_server_port = TCP_PORT;
    }
    else
    {
        g_server_port = port;
    }
}

void run_tcp_server(volatile bool *flg)
{
    static uint8_t prev_state = 0xFFu;
    uint8_t ir = getSn_IR(sn);
    setSn_IR(sn, ir); // Clear Socket-specific interrupt bits
    setSIR(0x01);
    uint8_t current_state = getSn_SR(sn);
    uint16_t len;

    if (current_state != prev_state)
    {
        char prntbuf[96];
        snprintf(prntbuf, sizeof(prntbuf),
                 "TCP state: %s (0x%02X) -> %s (0x%02X)\r\n",
                 socket_state_name(prev_state), prev_state,
                 socket_state_name(current_state), current_state);
        uart1_print(prntbuf);
        prev_state = current_state;
    }

    switch (current_state)
    {
    case SOCK_ESTABLISHED:
    {
        len = getSn_RX_RSR(sn);
        if (len > 0)
        {
            if (len > DATA_BUF_SIZE)
                len = DATA_BUF_SIZE;
            recv(sn, g_dat_buf, len);

            setSn_IR(sn, 0xFF); // Clear W5500 interrupt flags
            uint16_t parse_len = len;
            if (parse_len > 0 && g_dat_buf[parse_len - 1] == 'e')
            {
                parse_len--;
            }
            
            rest_parser(&g_dat_buf[0], parse_len);

            char response[] = "{\"result\": 64}";
            char http_resp[256];
            uint16_t body_len = strlen(response);

            int total_len = snprintf(http_resp, sizeof(http_resp),
                                     "HTTP/1.1 200 OK\r\n"
                                     "Content-Type: application/json\r\n"
                                     "Content-Length: %u\r\n"
                                     "Connection: close\r\n"
                                     "\r\n"
                                     "%s",
                                     body_len, response);

            send(sn, (uint8_t *)http_resp, total_len);
            
            // Gracefully disconnect and force-close socket
            disconnect(sn);
            close(sn);
            
            // Re-open socket to LISTEN immediately for the next request
            socket(sn, Sn_MR_TCP, g_server_port, 0x00);
            listen(sn);
        }
        *flg = false;
        break;
    }

    case SOCK_INIT:
        listen(sn);
        *flg = false;
        break;

    case SOCK_LISTEN:
        *flg = false;
        break;

    // Handle all closing/closed states to guarantee immediate recovery
    case SOCK_CLOSE_WAIT:
    case SOCK_CLOSED:
    case SOCK_FIN_WAIT:
    case SOCK_TIME_WAIT:
    case SOCK_CLOSING:
    default:
        close(sn);
        socket(sn, Sn_MR_TCP, g_server_port, 0x00);
        listen(sn);
        *flg = false;
        break;
    }
}
// --- Support Functions ---

void hardware_reset(void)
{
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN;
    W5500_RST_PORT->CRL &= ~(0xF << (W5500_RST_PIN * 4));
    W5500_RST_PORT->CRL |= (0x3 << (W5500_RST_PIN * 4));

    W5500_RST_PORT->BRR = (1 << W5500_RST_PIN);
    delay_ms(50);
    W5500_RST_PORT->BSRR = (1 << W5500_RST_PIN);
    delay_ms(100);
}

void w5500_port_init(void)
{
    reg_wizchip_cs_cbfunc(wizchip_select, wizchip_deselect);
    reg_wizchip_spi_cbfunc(spi_read, spi_write);
    reg_wizchip_spiburst_cbfunc(spi_read_burst, spi_write_burst);
}

static void delay_ms(uint32_t ms)
{
    while (ms--)
    {
        for (volatile uint32_t cycles = 0; cycles < 9000; ++cycles)
            __asm("nop");
    }
}