#ifndef _REST_PARSER_H_
#define _REST_PARSER_H_

#include "uart.h"
#include "stm32f1xx.h"
#include "string.h"
//#include "jsonParser.h"
#include "cJSON.h"
#include "cJSON_Utils.h"
#include "parser.h"


/* Parses a JSON command from buf_/len and writes "OK\r\n" or "WRONG COMMAND\r\n" into resp (if non-NULL). */
void rest_parser(uint8_t *buf_, uint16_t len, char *resp, uint16_t resp_size);

/* True if buf/len starts with an HTTP request line (GET/POST/PUT/DELETE/HEAD/PATCH/OPTIONS). */
int rest_parser_is_http_request(const uint8_t *buf, uint16_t len);

/* Runs an HTTP request's JSON body through rest_parser() and writes a full HTTP/1.1 response into http_resp. */
void rest_parser_http(uint8_t *buf_, uint16_t len, char *http_resp, uint16_t http_resp_size);


#endif
