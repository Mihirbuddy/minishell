#ifndef BUILTINS_HPP
#define BUILTINS_HPP

class Shell;

/*
 * Returns true if the given command is currently implemented
 * as a built-in command.
 */
bool isBuiltinCommand(const char *commandName);

/*
 * Executes a built-in command.
 *
 * Returns:
 *     0  on success
 *    -1  on failure
 */
int executeBuiltinCommand(char *command, Shell &shell);

int getBuiltinCommandCount();
const char *getBuiltinCommandName(int index);

#endif