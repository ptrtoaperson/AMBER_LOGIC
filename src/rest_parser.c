#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include "cJSON.h"
#include "rest_parser.h"
#include "interface.h"
#include "matrix_cfg.h"

#define DB_MAX      23
#define SAMTEC_MAX  3

float dac_volatges[14] = {0};
// Forward declaration of UART function
extern void uart1_print(const char *str);
static void jsonWalker(cJSON *root, int depth) {
    if (root == NULL) return;

    cJSON *element = NULL;
    char prntbuf[128];

    // Create dynamic visual indent spaces based on recursion depth
    char indent[16];
    int pad = (depth * 2 < (int)sizeof(indent) - 1) ? depth * 2 : (int)sizeof(indent) - 1;
    memset(indent, ' ', pad);
    indent[pad] = '\0';

    cJSON_ArrayForEach(element, root) {
        // Fallback name for array elements (since array items have NULL key strings)
        const char *key = (element->string != NULL) ? element->string : "item";

        // 1. Strings
        if (cJSON_IsString(element)) {
  /*          snprintf(prntbuf, sizeof(prntbuf), "%skey: %s, value: %s\r\n",
                     indent, key, element->valuestring);
            uart1_print(prntbuf);
    */
       }
        // 2. Numbers (Splits float into int.frac to guarantee nano.specs compatibility)
        else if (cJSON_IsNumber(element)) {
            double val = element->valuedouble;
            int integer_part = (int)val;
            int frac_part = (int)((val - integer_part) * 100);
            if (frac_part < 0) frac_part = -frac_part;

  /*          snprintf(prntbuf, sizeof(prntbuf), "%skey: %s, value: %d.%02d\r\n",
                     indent, key, integer_part, frac_part);
            uart1_print(prntbuf);
    */
       }
        // 3. Booleans
        else if (cJSON_IsBool(element)) {
 /*           snprintf(prntbuf, sizeof(prntbuf), "%skey: %s, value: %s\r\n",
                     indent, key, cJSON_IsTrue(element) ? "true" : "false");
            uart1_print(prntbuf);
    */
       }
        // 4. Sub-Objects (Recursive call)
        else if (cJSON_IsObject(element)) {
 /*           snprintf(prntbuf, sizeof(prntbuf), "%skey: %s (Object):\r\n", indent, key);
            uart1_print(prntbuf);
*/
            // Recurse down into child object
            jsonWalker(element, depth + 1);
        }
        // 5. Sub-Arrays (Recursive call)
        else if (cJSON_IsArray(element)) {
/*            snprintf(prntbuf, sizeof(prntbuf), "%skey: %s (Array size %d):\r\n",
                     indent, key, cJSON_GetArraySize(element));
            uart1_print(prntbuf);
*/
            // Recurse down into array elements
            jsonWalker(element, depth + 1);
        }
    }
}

/* Return value for each key parser: whether the key was present and, if so, whether it is valid. */
typedef enum {
    JSON_KEY_ABSENT = 0,
    JSON_KEY_OK = 1,
    JSON_KEY_ERROR = -1
} json_key_result_t;

/* A fully validated request. Nothing touches the hardware until every key has been parsed successfully. */
typedef struct {
    uint8_t has_mux;
    uint8_t mux_id;      /* 0 disables all mux lines */
    uint8_t has_address;
    uint16_t address;
    uint8_t has_enable;
    uint8_t enable;
    uint8_t has_config;
    matrix_mode_t mode;
    uint8_t config;
} json_cmd_t;

/* "Quadrant": null/"None"/"none" disables all mux lines, 1-4 selects a mux line. */
static json_key_result_t parse_mux_key(cJSON *root, json_cmd_t *cmd)
{
    cJSON *mux = cJSON_GetObjectItemCaseSensitive(root, "Quadrant");

    if (mux == NULL) {
        return JSON_KEY_ABSENT;
    }

    cmd->has_mux = 1u;

    if (cJSON_IsNull(mux)) {
        cmd->mux_id = 0u;
        return JSON_KEY_OK;
    }

    if (cJSON_IsString(mux) && mux->valuestring != NULL) {
        if (strcmp(mux->valuestring, "None") == 0 || strcmp(mux->valuestring, "none") == 0) {
            cmd->mux_id = 0u;
            return JSON_KEY_OK;
        }
        uart1_print("Quadrant: invalid string value (use \"None\")\r\n");
        return JSON_KEY_ERROR;
    }

    if (cJSON_IsNumber(mux)) {
        int mux_id = (int)mux->valuedouble;
        if (mux_id >= 1 && mux_id <= 4) {
            cmd->mux_id = (uint8_t)mux_id;
            return JSON_KEY_OK;
        }
        uart1_print("Quadrant: invalid id (use 1-4 or \"None\")\r\n");
        return JSON_KEY_ERROR;
    }

    uart1_print("Quadrant: unsupported value type\r\n");
    return JSON_KEY_ERROR;
}

/* "FportPairs": "F1F2"/"F3F4"/"F5F6"/"F7F8" maps to address[1:0] = 0..3. */
static int parse_fport_pairs(const char *name, uint8_t *bits)
{
    static const char *const pairs[] = {"F1F2", "F3F4", "F5F6", "F7F8"};
    uint8_t i;

    for (i = 0u; i < sizeof(pairs) / sizeof(pairs[0]); i++) {
        if (strcmp(name, pairs[i]) == 0) {
            *bits = i;
            return 0;
        }
    }
    return -1;
}

/* Reads an integer key that must lie in [0, max]. */
static int parse_bounded_int(cJSON *item, int max, int *out)
{
    int val;

    if (!cJSON_IsNumber(item)) {
        return -1;
    }
    val = (int)item->valuedouble;
    if (val < 0 || val > max) {
        return -1;
    }
    *out = val;
    return 0;
}

/* "DB" (0-23), "Samtec" (0-3) and "FportPairs" are required together and form
 * address = (DB << 5) | (Samtec << 2) | FportPairs.
 * "enable": 1 (enable) or "-" (disable). */
static json_key_result_t parse_address_enable_keys(cJSON *root, json_cmd_t *cmd)
{
    cJSON *db = cJSON_GetObjectItemCaseSensitive(root, "DB");
    cJSON *samtec = cJSON_GetObjectItemCaseSensitive(root, "Samtec");
    cJSON *fport = cJSON_GetObjectItemCaseSensitive(root, "FportPairs");
    cJSON *enable = cJSON_GetObjectItemCaseSensitive(root, "enable");
    int db_val;
    int samtec_val;
    uint8_t fport_bits = 0u;

    if (db == NULL && samtec == NULL && fport == NULL && enable == NULL) {
        return JSON_KEY_ABSENT;
    }

    if (enable != NULL) {
        if (cJSON_IsNumber(enable) && (int)enable->valuedouble == 1) {
            cmd->enable = 1u;
        } else if (cJSON_IsString(enable) && enable->valuestring != NULL
                   && strcmp(enable->valuestring, "-") == 0) {
            cmd->enable = 0u;
        } else {
            uart1_print("enable: invalid value (use 1 or \"-\")\r\n");
            return JSON_KEY_ERROR;
        }
        cmd->has_enable = 1u;
    }

    if (db == NULL && samtec == NULL && fport == NULL) {
        return JSON_KEY_OK;
    }

    if (db == NULL || samtec == NULL || fport == NULL) {
        uart1_print("DB/Samtec/FportPairs: all three are required together\r\n");
        return JSON_KEY_ERROR;
    }

    if (parse_bounded_int(db, DB_MAX, &db_val) != 0) {
        uart1_print("DB: invalid value (0-23)\r\n");
        return JSON_KEY_ERROR;
    }
    if (parse_bounded_int(samtec, SAMTEC_MAX, &samtec_val) != 0) {
        uart1_print("Samtec: invalid value (0-3)\r\n");
        return JSON_KEY_ERROR;
    }
    if (!cJSON_IsString(fport) || fport->valuestring == NULL
        || parse_fport_pairs(fport->valuestring, &fport_bits) != 0) {
        uart1_print("FportPairs: invalid value (use F1F2, F3F4, F5F6 or F7F8)\r\n");
        return JSON_KEY_ERROR;
    }

    cmd->address = (uint16_t)(((uint16_t)db_val << 5) | ((uint16_t)samtec_val << 2) | fport_bits);
    cmd->has_address = 1u;
    return JSON_KEY_OK;
}

/* "mode": "measurement"/"diagnostic" with "config": "config_N" (or the bare number N); both are required together. */
static json_key_result_t parse_mode_config_keys(cJSON *root, json_cmd_t *cmd)
{
    cJSON *mode = cJSON_GetObjectItemCaseSensitive(root, "mode");
    cJSON *config = cJSON_GetObjectItemCaseSensitive(root, "config");
    unsigned long cfg_id;

    if (mode == NULL && config == NULL) {
        return JSON_KEY_ABSENT;
    }

    if (mode == NULL || config == NULL) {
        uart1_print("mode/config: both keys are required together\r\n");
        return JSON_KEY_ERROR;
    }

    if (!cJSON_IsString(mode) || mode->valuestring == NULL) {
        uart1_print("mode: expected \"measurement\" or \"diagnostic\"\r\n");
        return JSON_KEY_ERROR;
    }
    if (strcmp(mode->valuestring, "measurement") == 0) {
        cmd->mode = MATRIX_MODE_MEASUREMENT;
    } else if (strcmp(mode->valuestring, "diagnostic") == 0) {
        cmd->mode = MATRIX_MODE_DIAGNOSTIC;
    } else {
        uart1_print("mode: invalid value (use \"measurement\" or \"diagnostic\")\r\n");
        return JSON_KEY_ERROR;
    }

    if (cJSON_IsString(config) && config->valuestring != NULL
        && strncmp(config->valuestring, "config_", 7) == 0) {
        char *end;
        const char *num = config->valuestring + 7;

        cfg_id = strtoul(num, &end, 10);
        if (end == num || *end != '\0') {
            uart1_print("config: invalid format (use \"config_N\")\r\n");
            return JSON_KEY_ERROR;
        }
    } else if (cJSON_IsNumber(config) && config->valuedouble >= 0.0) {
        cfg_id = (unsigned long)config->valuedouble;
    } else {
        uart1_print("config: invalid format (use \"config_N\")\r\n");
        return JSON_KEY_ERROR;
    }

    if (cfg_id < 1u || cfg_id > matrix_cfg_count(cmd->mode)) {
        uart1_print("config: out of range for mode\r\n");
        return JSON_KEY_ERROR;
    }

    cmd->config = (uint8_t)cfg_id;
    cmd->has_config = 1u;
    return JSON_KEY_OK;
}

/* Validates every key first and only then drives the hardware, so a bad request changes nothing.
 * An error or a payload with no recognized key is "wrong command". */
static int dispatch_json_commands(cJSON *root)
{
    json_cmd_t cmd;
    json_key_result_t mux_res;
    json_key_result_t addr_res;
    json_key_result_t cfg_res;

    memset(&cmd, 0, sizeof(cmd));
    mux_res = parse_mux_key(root, &cmd);
    addr_res = parse_address_enable_keys(root, &cmd);
    cfg_res = parse_mode_config_keys(root, &cmd);

    if (mux_res == JSON_KEY_ERROR || addr_res == JSON_KEY_ERROR || cfg_res == JSON_KEY_ERROR) {
        return -1;
    }

    if (mux_res == JSON_KEY_ABSENT && addr_res == JSON_KEY_ABSENT && cfg_res == JSON_KEY_ABSENT) {
        return -1;
    }

    if (cmd.has_mux) {
        mux_select(cmd.mux_id);
        uart1_print(cmd.mux_id ? "mux: selected\r\n" : "mux: disabled\r\n");
    }

    if (cmd.has_address) {
        set_address_enable_power(cmd.address, cmd.enable, 1u);
        uart1_print("address: applied\r\n");
    } else if (cmd.has_enable) {
        set_enable_lt(cmd.enable);
        uart1_print("enable: applied\r\n");
    }

    if (cmd.has_config) {
        if (matrix_apply_config(cmd.mode, cmd.config) != 0) {
            uart1_print("config: mapping rejected\r\n");
            return -1;
        }
        uart1_print("config: applied\r\n");
    }

    return 0;
}

void rest_parser(uint8_t *buf_, uint16_t len, char *resp, uint16_t resp_size) {
    uart1_print("reached rest parser\r\n");

    if (resp != NULL && resp_size > 0u) {
        snprintf(resp, resp_size, "WRONG COMMAND\r\n");
    }

    if (buf_ == NULL || len == 0) {
        uart1_print("Error: Empty or NULL buffer\r\n");
        return;
    }

    // 1. Safely find the start of the JSON payload '{'
    while (len > 0 && *buf_ != '{') {
        buf_++;
        len--;
    }

    if (len == 0) {
        uart1_print("Error: No '{' found in buffer\r\n");
        return;
    }

    uint8_t *pktdump = buf_;
    *(pktdump + len) = '\0';
    char prntbuf[128];
    snprintf(prntbuf, sizeof(prntbuf), "json len -> %u\r\n", len);
    uart1_print(prntbuf);



cJSON *root = cJSON_Parse(pktdump);
if (root) {
  if (!cJSON_IsObject(root)) {
        uart1_print("Error: Root JSON is not an object\r\n");
        cJSON_Delete(root);
        return;
    }
    jsonWalker(root, 0);
    if (dispatch_json_commands(root) == 0 && resp != NULL && resp_size > 0u) {
        snprintf(resp, resp_size, "OK\r\n");
    }
    cJSON_Delete(root);
}
else {
   const char *error_ptr = cJSON_GetErrorPtr();
        snprintf(prntbuf, sizeof(prntbuf), "cJSON Parse Error before: %s\r\n", 
                 error_ptr ? error_ptr : "unknown");
        uart1_print(prntbuf);
        return;
}
}

int rest_parser_is_http_request(const uint8_t *buf, uint16_t len)
{
    static const char *http_methods[] = {
        "GET ", "POST ", "PUT ", "DELETE ", "HEAD ", "PATCH ", "OPTIONS "
    };
    size_t i;

    if (buf == NULL) {
        return 0;
    }

    for (i = 0; i < sizeof(http_methods) / sizeof(http_methods[0]); i++) {
        size_t mlen = strlen(http_methods[i]);
        if ((size_t)len >= mlen && memcmp(buf, http_methods[i], mlen) == 0) {
            return 1;
        }
    }
    return 0;
}

/* Runs the request body (headers are skipped automatically, same as rest_parser())
 * through the JSON command dispatch and formats the result as a minimal HTTP/1.1 response. */
void rest_parser_http(uint8_t *buf_, uint16_t len, char *http_resp, uint16_t http_resp_size)
{
    char status[32];
    const char *body;

    rest_parser(buf_, len, status, sizeof(status));

    body = (strncmp(status, "OK", 2) == 0)
           ? "{\"status\":\"OK\"}"
           : "{\"status\":\"WRONG COMMAND\"}";

    snprintf(http_resp, http_resp_size,
             "HTTP/1.1 200 OK\r\n"
             "Content-Type: application/json\r\n"
             "Content-Length: %u\r\n"
             "Connection: keep-alive\r\n"  // <--- CHANGED FROM "close" to "keep-alive"
             "\r\n"
             "%s",
             (unsigned int)strlen(body), body);
}

/*
1. collect  key values
2. structure the commands
3. build a command table
4. execute the commad table via parser
*/
