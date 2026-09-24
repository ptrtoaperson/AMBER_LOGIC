#ifndef SHELL_COMMANDS_H_
#define SHELL_COMMANDS_H_

#include "shell.h"
#include "parser.h"

/* Compile-time command table, defined in shell_commands.c. Add new commands there. */
extern const shell_command_t g_shell_commands[];
extern const uint32_t g_shell_command_count;

/* Loads persisted BH/BL/EN/VCC/VSS/SUB DAC levels from flash, if valid. Call once at boot. */
void load_saved_levels(void);

#endif /* SHELL_COMMANDS_H_ */
