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

/* Resets the shell to its dormant (silent) state. It starts when Enter is pressed or "shell" is typed. */
void shell_init(void);

/* Leaves the interactive shell and returns to the dormant state. */
void shell_exit(void);

/* Feeds one received byte into the line editor. Safe to call from a UART rx callback/ISR. */
void shell_input_char(char c);

/* Call from the main loop; executes a completed line, if any, and reprints the prompt. */
void shell_process(void);

#endif /* SHELL_H_ */
