#include "shell.h"
#include "shell_commands.h"
#include "uart.h"
#include <string.h>

#define SHELL_PROMPT "\r\n> "

const char banner[] =
    "######################################################\r\n"
    "#                                                    #\r\n"
    "#   _____                 _    ___                   #\r\n"
    "#  / ____|               (_)  / _ \\                  #\r\n"
    "# | (___   ___ _ __ ___   _  | | | | ___  _ __       #\r\n"
    "#  \\___ \\ / _ \\ '_ ` _ \\ | | | | | |/ _ \\| '_ \\      #\r\n"
    "#  ____) |  __/ | | | | || | | |_| | (_) | | | |     #\r\n"
    "# |_____/ \\___|_| |_| |_||_|  \\___/ \\___/|_| |_|     #\r\n"
    "#                                \\\\                  #\r\n"
    "#           Automated Measurement system             #\r\n"
    "######################################################\r\n";

static char s_line_buf[SHELL_MAX_LINE_LEN];
static volatile uint16_t s_line_len = 0;
static volatile uint8_t s_line_ready = 0;
static volatile uint8_t s_active = 0;
static volatile uint8_t s_start_request = 0;

/* While dormant, Enter on an empty line or the word "shell" requests activation. Nothing is echoed. */
static void shell_dormant_input(char c)
{
    if (c == '\r' || c == '\n') {
        s_line_buf[s_line_len] = '\0';
        if (s_line_len == 0 || strcmp(s_line_buf, "shell") == 0) {
            s_start_request = 1;
        }
        s_line_len = 0;
        return;
    }

    if (c == '\b' || c == 0x7F) {
        if (s_line_len > 0) {
            s_line_len--;
        }
        return;
    }

    if (s_line_len < (SHELL_MAX_LINE_LEN - 1u)) {
        s_line_buf[s_line_len++] = c;
    }
}

static uint16_t shell_tokenize(char *line, char *argv[], uint16_t max_args)
{
    uint16_t argc = 0;
    char *p = line;

    while (*p != '\0' && argc < max_args) {
        while (*p == ' ') {
            p++;
        }
        if (*p == '\0') {
            break;
        }

        argv[argc++] = p;

        while (*p != '\0' && *p != ' ') {
            p++;
        }
        if (*p == ' ') {
            *p++ = '\0';
        }
    }

    return argc;
}

static void shell_execute_line(char *line)
{
    char *argv[SHELL_MAX_ARGS];
    uint16_t argc = shell_tokenize(line, argv, SHELL_MAX_ARGS);
    uint32_t i;

    if (argc == 0) {
        return;
    }

    for (i = 0; i < g_shell_command_count; i++) {
        if (strcmp(argv[0], g_shell_commands[i].name) == 0) {
            g_shell_commands[i].handler((int)argc, argv);
            return;
        }
    }

    uart1_print("\r\nUnknown command:");
    uart1_print(argv[0]);
    uart1_print("\r\n");
    uart1_print("\r\ntype \"help\" for list of avialable commands\r\n");
}

void shell_init(void)
{
    s_line_len = 0;
    s_line_ready = 0;
    s_active = 0;
    s_start_request = 0;
}

void shell_exit(void)
{
    s_line_len = 0;
    s_line_ready = 0;
    s_start_request = 0;
    s_active = 0;
}

void shell_input_char(char c)
{
    if (!s_active) {
        shell_dormant_input(c);
        return;
    }

    /* Drop input until the pending line has been consumed by shell_process(). */
    if (s_line_ready) {
        return;
    }

    if (c == '\r' || c == '\n') {
        s_line_buf[s_line_len] = '\0';
        s_line_ready = 1;
        return;
    }

    if (c == '\b' || c == 0x7F) {
        if (s_line_len > 0) {
            s_line_len--;
            uart1_print("\b \b");
        }
        return;
    }

    if (s_line_len >= (SHELL_MAX_LINE_LEN - 1u)) {
        return;
    }

    s_line_buf[s_line_len++] = c;
    uart1_write(c);
}

void shell_process(void)
{
    if (!s_active) {
        if (s_start_request) {
            s_start_request = 0;
            s_line_len = 0;
            s_line_ready = 0;
            s_active = 1;
            uart1_print(banner);
            uart1_print(SHELL_PROMPT);
        }
        return;
    }

    if (!s_line_ready) {
        return;
    }

    shell_execute_line(s_line_buf);

    s_line_len = 0;
    s_line_ready = 0;
    if (s_active) {
        uart1_print(SHELL_PROMPT);
    }
}
