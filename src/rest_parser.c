#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "cJSON.h"
#include "rest_parser.h"

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

void rest_parser(uint8_t *buf_, uint16_t len) {
    uart1_print("reached rest parser\r\n");

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

    // 2. Prepare a local stack buffer (sized down to 512 bytes to prevent STM32 stack overflow)
/*
    char pktdump[512];
    if (len >= sizeof(pktdump)) {
        len = sizeof(pktdump) - 1;
    }
*/
//    memcpy(pktdump, buf_, len);
  //  pktdump[len] = '\0'; // Require null-termination for cJSON
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

/*
1. collect  key values
2. structure the commands
3. build a command table
4. execute the commad table via parser
*/
