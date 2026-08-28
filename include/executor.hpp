#ifndef EXECUTOR_HPP
#define EXECUTOR_HPP

/*
 * Execute an external command.
 *
 * background:
 *     false -> parent waits for the child
 *     true  -> parent immediately displays the next prompt
 */
int executeExternalCommand(char *command, bool background);

/*
 * Reap background processes that have completed.
 */
void reapBackgroundProcesses();

#endif