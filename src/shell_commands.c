#include "shell_commands.h"
#include "uart.h"
#include "flash.h"
#include "lantask.h"
#include "interface.h"
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#define NETCFG_FLASH_ADDR 0x08007000u
#define NETCFG_MAGIC 0x4E455443u /* 'NETC' */

/* Must match the fallback wiz_NetInfo/port set up in main.c. */
#define DEFAULT_IP0 192u
#define DEFAULT_IP1 168u
#define DEFAULT_IP2 1u
#define DEFAULT_IP3 50u
#define DEFAULT_PORT TCP_PORT

/* Add new command handlers below, then list them in g_shell_commands[]. */

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

static int cmd_help(int argc, char *argv[])
{
    uint32_t i;

    (void)argc;
    (void)argv;

    uart1_print("\r\nAvailable commands:\r\n");
    for (i = 0; i < g_shell_command_count; i++) {
        uart1_print("  ");
        uart1_print(g_shell_commands[i].name);
        uart1_print(" - ");
        uart1_print(g_shell_commands[i].help ? g_shell_commands[i].help : "");
        uart1_print("\r\n");
    }

    return 0;
}

static int cmd_ping(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    uart1_print("\r\npong\r\n");
    return 0;
}

static int cmd_net(int argc, char *argv[]){
    (void)argc;
    (void)argv;
    uint32_t flash_data[3] = {0u, 0u, 0u};
    flash_read32(NETCFG_FLASH_ADDR, flash_data, 3u);

    bool netcfg_valid = (flash_data[2] == NETCFG_MAGIC)
                        && is_valid_ipv4_and_port(flash_data[0], flash_data[1]);

    if (netcfg_valid) {
        uint8_t ip0 = (uint8_t)((flash_data[0] >> 0) & 0xFFu);
        uint8_t ip1 = (uint8_t)((flash_data[0] >> 8) & 0xFFu);
        uint8_t ip2 = (uint8_t)((flash_data[0] >> 16) & 0xFFu);
        uint8_t ip3 = (uint8_t)((flash_data[0] >> 24) & 0xFFu);
        uint16_t port = (uint16_t)flash_data[1];

        char netbuf[96];
        snprintf(netbuf,
                 sizeof(netbuf),
                 "\r\nIP address: %u.%u.%u.%u\r\nPort: %u\r\n",
                 ip0, ip1, ip2, ip3, port);
        uart1_print(netbuf);
    } else {
        char defbuf[128];
        snprintf(defbuf,
                 sizeof(defbuf),
                 "Stored network config invalid/missing; using defaults\r\nIP address: %u.%u.%u.%u\r\nPort: %u\r\n",
                 DEFAULT_IP0, DEFAULT_IP1, DEFAULT_IP2, DEFAULT_IP3, (unsigned int)DEFAULT_PORT);
        uart1_print(defbuf);
    }
    return 0;
}

/* Parses one dotted-decimal octet starting at *p, advancing *p past the trailing separator. */
static int parse_ip_octet(char **p, char sep, uint8_t *out)
{
    char *end;
    unsigned long value = strtoul(*p, &end, 10);

    if (end == *p || value > 255u || *end != sep) {
        return -1;
    }

    *out = (uint8_t)value;
    *p = end + 1;
    return 0;
}

static int cmd_netcfg(int argc, char *argv[])
{
    uint8_t ip[4];
    char *p;
    char *end;
    unsigned long port_val;
    uint32_t packed_ip;
    uint32_t flash_data_write[3];
    uint32_t flash_verify[3] = {0u, 0u, 0u};
    char msg[160];

    if (argc != 3) {
        uart1_print("Usage: netcfg <ip> <port>\r\n");
        return -1;
    }

    p = argv[1];
    if (parse_ip_octet(&p, '.', &ip[0]) != 0
        || parse_ip_octet(&p, '.', &ip[1]) != 0
        || parse_ip_octet(&p, '.', &ip[2]) != 0) {
        uart1_print("Invalid IP address format\r\n");
        return -1;
    }
    {
        unsigned long last_octet = strtoul(p, &end, 10);
        if (end == p || *end != '\0' || last_octet > 255u) {
            uart1_print("\r\nInvalid IP address format\r\n");
            return -1;
        }
        ip[3] = (uint8_t)last_octet;
    }

    port_val = strtoul(argv[2], &end, 10);
    if (end == argv[2] || *end != '\0' || port_val == 0u || port_val > 65535u) {
        uart1_print("Invalid port\r\n");
        return -1;
    }

    packed_ip = ((uint32_t)ip[0] << 0) | ((uint32_t)ip[1] << 8)
              | ((uint32_t)ip[2] << 16) | ((uint32_t)ip[3] << 24);

    if (!is_valid_ipv4_and_port(packed_ip, port_val)) {
        uart1_print("\r\nIP/port rejected (loopback/broadcast/multicast/reserved not allowed)\r\n");
        return -1;
    }

    flash_data_write[0] = packed_ip;
    flash_data_write[1] = (uint32_t)port_val;
    flash_data_write[2] = NETCFG_MAGIC;

    flash_write32(NETCFG_FLASH_ADDR, flash_data_write, 3u);
    flash_read32(NETCFG_FLASH_ADDR, flash_verify, 3u);

    if (flash_verify[0] != flash_data_write[0]
        || flash_verify[1] != flash_data_write[1]
        || flash_verify[2] != flash_data_write[2]) {
        uart1_print("\r\nFlash verify FAILED\r\n");
        return -1;
    }

    apply_network_config(ip, (uint16_t)port_val);

    snprintf(msg,
             sizeof(msg),
             "\r\nStored IP %u.%u.%u.%u, port %lu to flash and applied\r\n",
             ip[0], ip[1], ip[2], ip[3], port_val);
    uart1_print(msg);

    return 0;
}

static int cmd_setaddress(int argc, char *argv[])
{
    char *end;
    unsigned long address_val;
    unsigned long en_val;

    if (argc != 3) {
        uart1_print("Usage: setaddress <address> <en 0|1>\r\n");
        return -1;
    }

    address_val = strtoul(argv[1], &end, 10);
    if (end == argv[1] || *end != '\0' || address_val > 0xFFFFu) {
        uart1_print("Invalid address\r\n");
        return -1;
    }

    en_val = strtoul(argv[2], &end, 10);
    if (end == argv[2] || *end != '\0' || (en_val != 0u && en_val != 1u)) {
        uart1_print("Invalid enable bit (use 0 or 1)\r\n");
        return -1;
    }

    set_address((uint16_t)address_val);
    set_enable_lt((uint8_t)en_val);

    uart1_print("\r\nAddress applied\r\n");
    return 0;
}

static int cmd_setpower(int argc, char *argv[])
{
    if (argc != 2) {
        uart1_print("Usage: setpower <on|off>\r\n");
        return -1;
    }

    if (strcmp(argv[1], "on") == 0) {
        set_power(1u);
    } else if (strcmp(argv[1], "off") == 0) {
        set_power(0u);
    } else {
        uart1_print("Invalid argument (use on or off)\r\n");
        return -1;
    }

    uart1_print("\r\nPower applied\r\n");
    return 0;
}

static int cmd_config_matrix(int argc, char *argv[])
{
    uint8_t pair_data[2u * (SHELL_MAX_ARGS - 1u)];
    uint8_t pair_count;
    int i;

    if (argc < 2) {
        uart1_print("Usage: config_matrix <row,col> [row,col ...]\r\n");
        return -1;
    }

    pair_count = (uint8_t)(argc - 1);

    for (i = 0; i < (int)pair_count; i++) {
        char *p = argv[i + 1];
        char *end;
        unsigned long row = strtoul(p, &end, 10);
        unsigned long col;

        if (end == p || *end != ',') {
            uart1_print("Invalid pair format (use row,col)\r\n");
            return -1;
        }
        col = strtoul(end + 1, &end, 10);
        if (*end != '\0' || row > 0xFFu || col > 0xFFu) {
            uart1_print("Invalid pair format (use row,col)\r\n");
            return -1;
        }

        pair_data[i * 2] = (uint8_t)row;
        pair_data[i * 2 + 1] = (uint8_t)col;
    }

    if (matrix_set_pairs_mapping(pair_data, pair_count) != 0) {
        uart1_print("\r\nMatrix mapping rejected (check row/col ranges)\r\n");
        return -1;
    }

    uart1_print("\r\nMatrix mapping applied\r\n");
    return 0;
}

static int cmd_setmux(int argc, char *argv[])
{
    char *end;
    unsigned long mux_val;

    if (argc != 2) {
        uart1_print("Usage: setmux <n>|none\r\n");
        return -1;
    }

    if (strcmp(argv[1], "none") == 0) {
        mux_select(0u);
        uart1_print("\r\nMux disabled\r\n");
        return 0;
    }

    mux_val = strtoul(argv[1], &end, 10);
    if (end == argv[1] || *end != '\0' || mux_val > 0xFFu) {
        uart1_print("Invalid mux id\r\n");
        return -1;
    }

    mux_select((uint8_t)mux_val);

    uart1_print("\r\nMux selected\r\n");
    return 0;
}

const shell_command_t g_shell_commands[] = {
    { "help", cmd_help, "list available commands" },
    { "ping", cmd_ping, "check shell responsiveness" },
    { "net", cmd_net, "show the network settings" },
    { "netcfg", cmd_netcfg, "configure the network settings: netcfg <ip> <port>"},
    { "setaddress", cmd_setaddress, "setaddress <address> <en 0|1>" },
    { "setpower", cmd_setpower, "setpower <on|off>" },
    { "config_matrix", cmd_config_matrix, "config_matrix <row,col> [row,col ...]" },
    { "setmux", cmd_setmux, "setmux <n>|none" }
};

const uint32_t g_shell_command_count = sizeof(g_shell_commands) / sizeof(g_shell_commands[0]);
