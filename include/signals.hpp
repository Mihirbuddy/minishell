#ifndef SIGNALS_HPP
#define SIGNALS_HPP

#include <sys/types.h>

/*
 * Install Ctrl+C and Ctrl+Z handlers for the shell.
 */
bool initializeSignalHandlers();

/*
 * Restore normal signal behaviour inside a child process.
 */
void restoreDefaultSignalHandlers();

/*
 * Record the process group currently running in the foreground.
 */
void setForegroundProcessGroup(pid_t processGroupId);

/*
 * The shell no longer has a foreground process group.
 */
void clearForegroundProcessGroup();

#endif