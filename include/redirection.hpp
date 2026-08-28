#ifndef REDIRECTION_HPP
#define REDIRECTION_HPP

#include <limits.h>
#include <cstddef>

struct RedirectionInfo
{
  bool hasInput;
  bool hasOutput;
  bool appendOutput;

  char inputFile[PATH_MAX];
  char outputFile[PATH_MAX];
};

struct SavedFileDescriptors
{
  int standardInput;
  int standardOutput;
};

/*
 * Extracts <, > and >> from the command.
 *
 * originalCommand:
 *     echo hello > output.txt
 *
 * executableCommand:
 *     echo hello
 */
bool parseRedirections(const char *originalCommand,
                       char *executableCommand,
                       size_t executableCommandSize,
                       RedirectionInfo &information);

/*
 * Applies redirection using open() and dup2().
 *
 * saveOriginalDescriptors should be true for built-ins executed
 * in the parent shell and false for child processes.
 */
bool applyRedirections(
    const RedirectionInfo &information,
    bool saveOriginalDescriptors,
    SavedFileDescriptors &savedDescriptors);

/*
 * Restores the shell's original stdin and stdout.
 */
void restoreRedirections(
    SavedFileDescriptors &savedDescriptors);

#endif