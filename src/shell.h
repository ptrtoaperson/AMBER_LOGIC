#ifndef SHELL_H_
#define SHELL_H_

#include <stdint.h>

#define SHELL_MAX_LINE_LEN   64u
#define SHELL_MAX_ARGS       8u


typedef int (*shell_cmd_fn)(int argc, char *argv[]);

typedef struct {
    const char   *name;
    shell_cmd_fn  handler;
    const char   *help;
} shell_command_t;

/* Resets the line editor and prints the initial prompt. Commands come from shell_commands.c. */
void shell_init(void);

/* Feeds one received byte into the line editor. Safe to call from a UART rx callback/ISR. */
void shell_input_char(char c);

/* Call from the main loop; executes a completed line, if any, and reprints the prompt. */
void shell_process(void);

#endif /* SHELL_H_ */
