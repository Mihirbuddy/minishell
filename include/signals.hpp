#ifndef SIGNALS_HPP
#define SIGNALS_HPP

#include <sys/types.h>

bool initializeSignalHandlers();

void restoreDefaultSignalHandlers();

void setForegroundProcessGroup(pid_t processGroupId);

void clearForegroundProcessGroup();

/*
 * Give terminal control to a foreground command or pipeline.
 */
void giveTerminalTo(pid_t processGroupId);

/*
 * Return terminal control to the custom shell.
 */
void takeTerminalBack();

#endif