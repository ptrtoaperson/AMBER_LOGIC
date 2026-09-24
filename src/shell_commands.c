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

/* Separate flash page from NETCFG_FLASH_ADDR so saving levels never erases the network config. */
#define LEVELS_FLASH_ADDR 0x08020000u
#define LEVELS_MAGIC 0x4C56454Cu /* 'LVEL' */

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

/* code = ((volts+10)/20)*65536, clamped to a valid 16-bit DAC code (+/-10V bipolar span). */
static uint16_t volts_to_code(float volts)
{
    float scaled = ((volts + 10.0f) / 20.0f) * 65536.0f;

    if (scaled < 0.0f) {
        scaled = 0.0f;
    }
    if (scaled > 65535.0f) {
        scaled = 65535.0f;
    }

    return (uint16_t)(scaled + 0.5f);
}

/* Inverse of volts_to_code(), returned in millivolts (int) since nano.specs printf lacks %f. */
static int32_t code_to_millivolts(uint16_t code)
{
    float volts = ((float)code / 65536.0f) * 20.0f - 10.0f;
    return (int32_t)(volts * 1000.0f);
}

void load_saved_levels(void)
{
    uint32_t flash_data[7] = {0u, 0u, 0u, 0u, 0u, 0u, 0u};

    flash_read32(LEVELS_FLASH_ADDR, flash_data, 7u);

    if (flash_data[6] != LEVELS_MAGIC) {
        return;
    }

    BIT_HIGH = (uint16_t)flash_data[0];
    BIT_LOW  = (uint16_t)flash_data[1];
    EN_LT    = (uint16_t)flash_data[2];
    VCC      = (uint16_t)flash_data[3];
    VSS      = (uint16_t)flash_data[4];
    SUB      = (uint16_t)flash_data[5];
}

static int cmd_configlevels(int argc, char *argv[])
{
    int i;
    bool any_key = false;
    uint32_t flash_data[7];
    uint32_t flash_verify[7] = {0u, 0u, 0u, 0u, 0u, 0u, 0u};

    if (argc < 2) {
        uart1_print("Usage: configlevels BH=<v> BL=<v> EN=<v> VCC=<v> VSS=<v> SUB=<v>\r\n");
        return -1;
    }

    for (i = 1; i < argc; i++) {
        char *eq = strchr(argv[i], '=');
        char *end;
        float volts;

        if (eq == NULL) {
            uart1_print("Invalid argument (expected KEY=value)\r\n");
            return -1;
        }

        volts = strtof(eq + 1, &end);
        if (end == eq + 1 || *end != '\0') {
            uart1_print("Invalid voltage value\r\n");
            return -1;
        }

        if (strncmp(argv[i], "BH=", 3) == 0) {
            BIT_HIGH = volts_to_code(volts);
        } else if (strncmp(argv[i], "BL=", 3) == 0) {
            BIT_LOW = volts_to_code(volts);
        } else if (strncmp(argv[i], "EN=", 3) == 0) {
            EN_LT = volts_to_code(volts);
        } else if (strncmp(argv[i], "VCC=", 4) == 0) {
            VCC = volts_to_code(volts);
        } else if (strncmp(argv[i], "VSS=", 4) == 0) {
            VSS = volts_to_code(volts);
        } else if (strncmp(argv[i], "SUB=", 4) == 0) {
            SUB = volts_to_code(volts);
        } else {
            uart1_print("Unknown key (use BH, BL, EN, VCC, VSS, SUB)\r\n");
            return -1;
        }
        any_key = true;
    }

    if (!any_key) {
        uart1_print("Usage: configlevels BH=<v> BL=<v> EN=<v> VCC=<v> VSS=<v> SUB=<v>\r\n");
        return -1;
    }

    flash_data[0] = BIT_HIGH;
    flash_data[1] = BIT_LOW;
    flash_data[2] = EN_LT;
    flash_data[3] = VCC;
    flash_data[4] = VSS;
    flash_data[5] = SUB;
    flash_data[6] = LEVELS_MAGIC;

    flash_write32(LEVELS_FLASH_ADDR, flash_data, 7u);
    flash_read32(LEVELS_FLASH_ADDR, flash_verify, 7u);

    if (memcmp(flash_data, flash_verify, sizeof(flash_data)) != 0) {
        uart1_print("\r\nFlash verify FAILED\r\n");
        return -1;
    }

    interface_reapply();

    uart1_print("\r\nLevels stored and applied\r\n");
    return 0;
}

static int cmd_showlevels(int argc, char *argv[])
{
    char buf[192];

    (void)argc;
    (void)argv;

    snprintf(buf, sizeof(buf),
             "\r\nBH=%ldmV BL=%ldmV EN=%ldmV VCC=%ldmV VSS=%ldmV SUB=%ldmV\r\n",
             (long)code_to_millivolts(BIT_HIGH),
             (long)code_to_millivolts(BIT_LOW),
             (long)code_to_millivolts(EN_LT),
             (long)code_to_millivolts(VCC),
             (long)code_to_millivolts(VSS),
             (long)code_to_millivolts(SUB));
    uart1_print(buf);
    return 0;
}

const shell_command_t g_shell_commands[] = {
    { "help", cmd_help, "list available commands" },
    { "ping", cmd_ping, "check shell responsiveness" },
    { "net", cmd_net, "show the network settings" },
    { "netcfg", cmd_netcfg, "configure the network settings: netcfg <ip> <port>"},
    { "configlevels", cmd_configlevels, "configlevels BH=<logic high voltage> BL=<logic low voltage> EN=<enable voltage> VCC=<vcc voltage> VSS=<vss voltage> SUB=<substrate voltage>" },
    { "showlevels", cmd_showlevels, "show the configured BH/BL/EN/VCC/VSS/SUB levels" },
    { "setaddress", cmd_setaddress, "setaddress <address> <en 0|1>" },
    { "setpower", cmd_setpower, "setpower <on|off>" },
    { "config_matrix", cmd_config_matrix, "config_matrix <row,col> [row,col ...]" },
    { "setmux", cmd_setmux, "setmux <n>|none" }
};

const uint32_t g_shell_command_count = sizeof(g_shell_commands) / sizeof(g_shell_commands[0]);
