#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include "cJSON.h"
#include "rest_parser.h"
#include "interface.h"

#define MATRIX_JSON_MAX_PAIRS 9u

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
            snprintf(prntbuf, sizeof(prntbuf), "%skey: %s, value: %s\r\n",
                     indent, key, element->valuestring);
            uart1_print(prntbuf);
        }
        // 2. Numbers (Splits float into int.frac to guarantee nano.specs compatibility)
        else if (cJSON_IsNumber(element)) {
            double val = element->valuedouble;
            int integer_part = (int)val;
            int frac_part = (int)((val - integer_part) * 100);
            if (frac_part < 0) frac_part = -frac_part;

            snprintf(prntbuf, sizeof(prntbuf), "%skey: %s, value: %d.%02d\r\n",
                     indent, key, integer_part, frac_part);
            uart1_print(prntbuf);
        }
        // 3. Booleans
        else if (cJSON_IsBool(element)) {
            snprintf(prntbuf, sizeof(prntbuf), "%skey: %s, value: %s\r\n",
                     indent, key, cJSON_IsTrue(element) ? "true" : "false");
            uart1_print(prntbuf);
        }
        // 4. Sub-Objects (Recursive call)
        else if (cJSON_IsObject(element)) {
            snprintf(prntbuf, sizeof(prntbuf), "%skey: %s (Object):\r\n", indent, key);
            uart1_print(prntbuf);

            // Recurse down into child object
            jsonWalker(element, depth + 1);
        }
        // 5. Sub-Arrays (Recursive call)
        else if (cJSON_IsArray(element)) {
            snprintf(prntbuf, sizeof(prntbuf), "%skey: %s (Array size %d):\r\n",
                     indent, key, cJSON_GetArraySize(element));
            uart1_print(prntbuf);

            // Recurse down into array elements
            jsonWalker(element, depth + 1);
        }
    }
}

/* Return value for each key handler: whether the key was present and, if so, whether it applied cleanly. */
typedef enum {
    JSON_KEY_ABSENT = 0,
    JSON_KEY_OK = 1,
    JSON_KEY_ERROR = -1
} json_key_result_t;

/* "mux": "None"/"none" disables all mux lines, 1-4 selects a mux line. */
static json_key_result_t handle_mux_key(cJSON *root)
{
    cJSON *mux = cJSON_GetObjectItemCaseSensitive(root, "mux");

    if (mux == NULL) {
        return JSON_KEY_ABSENT;
    }

    if (cJSON_IsNull(mux)) {
        mux_select(0u);
        uart1_print("mux: disabled\r\n");
        return JSON_KEY_OK;
    }

    if (cJSON_IsString(mux) && mux->valuestring != NULL) {
        if (strcmp(mux->valuestring, "None") == 0 || strcmp(mux->valuestring, "none") == 0) {
            mux_select(0u);
            uart1_print("mux: disabled\r\n");
            return JSON_KEY_OK;
        }
        uart1_print("mux: invalid string value (use \"None\")\r\n");
        return JSON_KEY_ERROR;
    }

    if (cJSON_IsNumber(mux)) {
        int mux_id = (int)mux->valuedouble;
        if (mux_id >= 1 && mux_id <= 4) {
            mux_select((uint8_t)mux_id);
            uart1_print("mux: selected\r\n");
            return JSON_KEY_OK;
        }
        uart1_print("mux: invalid id (use 1-4 or \"None\")\r\n");
        return JSON_KEY_ERROR;
    }

    uart1_print("mux: unsupported value type\r\n");
    return JSON_KEY_ERROR;
}

/* "address": integer 0-1023. "enable": 1 (enable) or "-" (disable). */
static json_key_result_t handle_address_enable_keys(cJSON *root)
{
    cJSON *address = cJSON_GetObjectItemCaseSensitive(root, "address");
    cJSON *enable = cJSON_GetObjectItemCaseSensitive(root, "enable");
    uint8_t en = 0u;

    if (address == NULL && enable == NULL) {
        return JSON_KEY_ABSENT;
    }

    if (enable != NULL) {
        if (cJSON_IsNumber(enable) && (int)enable->valuedouble == 1) {
            en = 1u;
        } else if (cJSON_IsString(enable) && enable->valuestring != NULL
                   && strcmp(enable->valuestring, "-") == 0) {
            en = 0u;
        } else {
            uart1_print("enable: invalid value (use 1 or \"-\")\r\n");
            return JSON_KEY_ERROR;
        }
    }

    if (address == NULL) {
        set_enable_lt(en);
        uart1_print("enable: applied\r\n");
        return JSON_KEY_OK;
    }

    if (!cJSON_IsNumber(address)) {
        uart1_print("address: invalid type (expected integer)\r\n");
        return JSON_KEY_ERROR;
    }

    {
        int addr_val = (int)address->valuedouble;
        if (addr_val < 0 || addr_val >= 1024) {
            uart1_print("address: out of range (0-1023)\r\n");
            return JSON_KEY_ERROR;
        }

        set_address((uint16_t)addr_val);
        set_enable_lt(en);
        set_power(0x1);
        uart1_print("address: applied\r\n");
        return JSON_KEY_OK;
    }
}

/* "matrix": object of {"row": col, ...} pairs, e.g. {"1": 2, "3": 4}. */
static json_key_result_t handle_matrix_key(cJSON *root)
{
    cJSON *matrix = cJSON_GetObjectItemCaseSensitive(root, "matrix");
    cJSON *pair = NULL;
    uint8_t pair_data[2u * MATRIX_JSON_MAX_PAIRS];
    uint8_t pair_count = 0u;

    if (matrix == NULL) {
        return JSON_KEY_ABSENT;
    }

    if (!cJSON_IsObject(matrix)) {
        uart1_print("matrix: expected an object of row:col pairs\r\n");
        return JSON_KEY_ERROR;
    }

    cJSON_ArrayForEach(pair, matrix) {
        char *end;
        unsigned long row;
        long col;

        if (pair_count >= MATRIX_JSON_MAX_PAIRS) {
            uart1_print("matrix: too many pairs, truncating\r\n");
            break;
        }

        if (pair->string == NULL || !cJSON_IsNumber(pair)) {
            uart1_print("matrix: invalid pair entry\r\n");
            return JSON_KEY_ERROR;
        }

        row = strtoul(pair->string, &end, 10);
        if (end == pair->string || *end != '\0' || row > 0xFFu) {
            uart1_print("matrix: invalid row key\r\n");
            return JSON_KEY_ERROR;
        }

        col = (long)pair->valuedouble;
        if (col < 0 || col > 0xFF) {
            uart1_print("matrix: invalid column value\r\n");
            return JSON_KEY_ERROR;
        }

        pair_data[pair_count * 2u] = (uint8_t)row;
        pair_data[pair_count * 2u + 1u] = (uint8_t)col;
        pair_count++;
    }

    if (pair_count == 0u) {
        return JSON_KEY_ABSENT;
    }

    if (matrix_set_pairs_mapping(pair_data, pair_count) != 0) {
        uart1_print("matrix: mapping rejected (check row/col ranges)\r\n");
        return JSON_KEY_ERROR;
    }

    uart1_print("matrix: mapping applied\r\n");
    return JSON_KEY_OK;
}

/* Applies every recognized key and reports overall success; any error or unrecognized payload is "wrong command". */
static int dispatch_json_commands(cJSON *root)
{
    json_key_result_t mux_res = handle_mux_key(root);
    json_key_result_t addr_res = handle_address_enable_keys(root);
    json_key_result_t matrix_res = handle_matrix_key(root);

    if (mux_res == JSON_KEY_ERROR || addr_res == JSON_KEY_ERROR || matrix_res == JSON_KEY_ERROR) {
        return -1;
    }

    if (mux_res == JSON_KEY_ABSENT && addr_res == JSON_KEY_ABSENT && matrix_res == JSON_KEY_ABSENT) {
        return -1;
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
             "Connection: close\r\n"
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
