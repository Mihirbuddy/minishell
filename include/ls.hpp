#ifndef LS_HPP
#define LS_HPP

class Shell;

/*
 * Executes the custom ls built-in command.
 *
 * Returns:
 *      0 on success
 *     -1 on failure
 */
int executeLs(char *command, Shell &shell);

#endif