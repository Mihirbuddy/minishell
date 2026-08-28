#ifndef HISTORY_HPP
#define HISTORY_HPP

#include <cstddef>

/*
 * Load persistent history from the shell's home directory.
 */
bool initializeHistory(const char *homeDirectory);

/*
 * Add a command and save the updated history to disk.
 */
bool addHistoryCommand(const char *command);

/*
 * Print the latest numberOfCommands commands.
 */
void printHistory(int numberOfCommands);

int getHistoryCount();

const char *getHistoryCommand(int index);

#endif