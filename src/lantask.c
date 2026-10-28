#include "lantask.h"
#include "parser.h"
#include "rest_parser.h"
#include <stdio.h>
#include <string.h>

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

/* Pushes a new IP/port to the W5500 and restarts the listening socket immediately. */
void apply_network_config(const uint8_t ip[4], uint16_t port)
{
    wiz_NetInfo netinfo;
    uint8_t state;

    wizchip_getnetinfo(&netinfo);
    netinfo.ip[0] = ip[0];
    netinfo.ip[1] = ip[1];
    netinfo.ip[2] = ip[2];
    netinfo.ip[3] = ip[3];
    wizchip_setnetinfo(&netinfo);

    set_tcp_server_port(port);

    state = getSn_SR(sn);
    if (state == SOCK_ESTABLISHED || state == SOCK_CLOSE_WAIT)
    {
        disconnect(sn);
    }
    close(sn);
    socket(sn, Sn_MR_TCP, g_server_port, 0x00);
    listen(sn);
}
#if 0
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
        snprintf(prntbuf,
                 sizeof(prntbuf),
                 "TCP state: %s (0x%02X) -> %s (0x%02X)\r\n",
                 socket_state_name(prev_state),
                 prev_state,
                 socket_state_name(current_state),
                 current_state);
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
            if (len > DATA_BUF_SIZE - 1u)
                len = DATA_BUF_SIZE - 1u;
                
            int32_t received = recv(sn, g_dat_buf, len);
            if (received > 0)
            {
                g_dat_buf[received] = '\0'; // Safely null-terminate based on actual bytes received
                
                char prntbuf[100];
                snprintf(prntbuf, sizeof(prntbuf), "recv=%ld\r\n", (long)received);
                uart1_print(prntbuf);

                setSn_IR(sn, 0xFF); // Clear W5500 interrupt

                if (rest_parser_is_http_request(g_dat_buf, received))
                {
                    char http_resp[192];
                    
                    // NOTE: Ensure your rest_parser_http builds a valid HTTP/1.1 response 
                    // containing "Connection: keep-alive" and a proper "Content-Length" 
                    // since the connection is now persistent!
                    rest_parser_http(g_dat_buf, received, http_resp, sizeof(http_resp));
                    uart1_print("Data parsed (http)!\r\n");
                    
                    send(sn, (uint8_t *)http_resp, (uint16_t)strlen(http_resp));

                    /* 
                     * Persistent Connection Enabled: 
                     * We leave the socket open in SOCK_ESTABLISHED so the client 
                     * can reuse this exact connection for subsequent requests.
                     */
                }
                else if (g_dat_buf[0] == '{')
                {
                    char resp[32];
                    rest_parser(g_dat_buf, received, resp, sizeof(resp));
                    uart1_print("Data parsed (json)!\r\n");
                    send(sn, (uint8_t *)resp, (uint16_t)strlen(resp));
                }
                else
                {
                    parse_command(g_dat_buf, received);
                    uart1_print("Data parsed!\r\n");
                    send(sn, (uint8_t *)"OK", 2);
                }
            }
        }
        break;
    }
        
    case SOCK_CLOSE_WAIT:
    {
        // Peer requested closure; clean up safely
        uart1_print("Socket closing cleanup...\r\n");
        disconnect(sn);
        close(sn);
        socket(sn, Sn_MR_TCP, g_server_port, 0x00);
        listen(sn);

        if (flg) *flg = false;
        break;
    }

    case SOCK_CLOSED:
        socket(sn, Sn_MR_TCP, g_server_port, 0x00);
        break;

    case SOCK_INIT:
        listen(sn);
        break;

    case SOCK_LISTEN:
        if (flg) *flg = false;
        break;

    default:
        if (flg) *flg = false;
        break;
    }
}
#else
void run_tcp_server(volatile bool *flg)
{
    uart1_print("reached tcp server\r\n");
    static uint8_t prev_state = 0xFFu;

    // Clear everything except SEND_OK: send() in socket.c polls it to know the previous
    // transmission finished, and returns SOCK_BUSY forever if it was wiped here.
    uint8_t ir = getSn_IR(sn);
    setSn_IR(sn, ir & (uint8_t)~Sn_IR_SENDOK);

    uart1_print("w5500 interrrupt released!!\r\n");

    uint8_t current_state = getSn_SR(sn);
    uint16_t len;
    char prntbuf[96];
    snprintf(prntbuf, sizeof(prntbuf), "TCP state: %s -> %s\r\n",
             socket_state_name(prev_state), socket_state_name(current_state));
    uart1_print(prntbuf);
    uart1_print("**********************\r\n");

    if (current_state != prev_state)
    {
        char prntbuf[96];
        snprintf(prntbuf, sizeof(prntbuf), "TCP state: %s -> %s\r\n",
                 socket_state_name(prev_state), socket_state_name(current_state));
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
            if (len > DATA_BUF_SIZE - 1u)
                len = DATA_BUF_SIZE - 1u;

            int32_t received = recv(sn, g_dat_buf, len);
            if (received > 0)
            {
                g_dat_buf[received] = '\0'; // Null-terminate safely

                char prntbuf[64];
                snprintf(prntbuf, sizeof(prntbuf), "Received bytes: %ld\r\n", (long)received);
                uart1_print(prntbuf);

                // Process HTTP request
                if (rest_parser_is_http_request(g_dat_buf, (uint16_t)received))
                {
                    char http_resp[192];
                    rest_parser_http(g_dat_buf, (uint16_t)received, http_resp, sizeof(http_resp));
                    uint16_t resp_len = (uint16_t)strlen(http_resp);

                    // --- CHECK W5500 TX FREE BUFFER USING getsockopt ---
                    uint16_t freesize = 0;
                    uint32_t timeout = 50000; // safety timeout counter

                    // Query W5500 for available TX buffer space
                    do
                    {
                        getsockopt(sn, SO_SENDBUF, &freesize);
                        timeout--;
                    } while (freesize < resp_len && timeout > 0);

                    if (freesize >= resp_len)
                    {
                        int32_t sent_bytes = send(sn, (uint8_t *)http_resp, resp_len);
                        if (sent_bytes > 0)
                        {
                            uart1_print("HTTP Request processed & response sent!\r\n");
                        }
                        else
                        {
                            uart1_print("Error: Send failed!\r\n");
                        }
                    }
                    else
                    {
                        uart1_print("Error: W5500 TX buffer full (timeout)!\r\n");
                    }
                }
                else
                {
                    uart1_print("Warning: Received non-HTTP packet or partial chunk\r\n");
                }
            }
        }

        // Always clear the flag so the main loop unblocks
        if (flg)
            *flg = false;
        break;
    }

    case SOCK_CLOSE_WAIT:
    {
        // The client closed the connection; we must finish the teardown to free the socket!
        uart1_print("Peer closed connection, cleaning up...\r\n");
        disconnect(sn);
        close(sn);
        socket(sn, Sn_MR_TCP, g_server_port, 0x00);
        listen(sn);

        if (flg)
            *flg = false;
        break;
    }

    case SOCK_CLOSED:
        // The peer reset or the link timed out: re-open and listen right away, because
        // nothing else raises an interrupt to bring us back for the SOCK_INIT step.
        if (socket(sn, Sn_MR_TCP, g_server_port, 0x00) == sn)
        {
            listen(sn);
        }
        if (flg)
            *flg = false;
        break;

    case SOCK_INIT:
        listen(sn);
        if (flg)
            *flg = false;
        break;

    case SOCK_LISTEN:
        if (flg)
            *flg = false;
        break;

    default:
        // Transient/half-closed states (FIN_WAIT, CLOSING, TIME_WAIT, LAST_ACK) raise no
        // further interrupt we handle; reset the socket so the server can accept again.
        close(sn);
        if (socket(sn, Sn_MR_TCP, g_server_port, 0x00) == sn)
        {
            listen(sn);
        }
        if (flg)
            *flg = false;
        break;
    }
}
#endif
// --- Support Functions ---

void hardware_reset(void)
{
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN;
    W5500_RST_PORT->CRL &= ~(0xF << (W5500_RST_PIN * 4));
    W5500_RST_PORT->CRL |= (0x3 << (W5500_RST_PIN * 4));

    W5500_RST_PORT->BRR = (1 << W5500_RST_PIN);
    delay_ms(10);
    W5500_RST_PORT->BSRR = (1 << W5500_RST_PIN);
    delay_ms(20);
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