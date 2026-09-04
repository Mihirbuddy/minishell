#include "executor.hpp"
#include "redirection.hpp"
#include "signals.hpp"

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define MAX_ARGUMENTS 256

static int createArgumentArray(char *command,
                               char *arguments[],
                               int maximumArguments)
{
  if (command == NULL ||
      arguments == NULL ||
      maximumArguments <= 1)
  {
    return -1;
  }

  int argumentCount = 0;

  char *token = strtok(command, " \t");

  while (token != NULL)
  {
    /*
     * Keep one final position for the NULL pointer required
     * by execvp().
     */
    if (argumentCount >= maximumArguments - 1)
    {
      fprintf(stderr, "Too many command arguments\n");
      return -1;
    }

    arguments[argumentCount] = token;
    argumentCount++;

    token = strtok(NULL, " \t");
  }

  arguments[argumentCount] = NULL;

  return argumentCount;
}

int executeExternalCommand(
    char *command,
    bool background,
    const RedirectionInfo &redirection)
{
  if (command == NULL)
  {
    return -1;
  }

  char *arguments[MAX_ARGUMENTS];

  int argumentCount = createArgumentArray(
      command,
      arguments,
      MAX_ARGUMENTS);

  if (argumentCount <= 0)
  {
    return -1;
  }

  pid_t childPid = fork();

  if (childPid < 0)
  {
    perror("fork");
    return -1;
  }

  if (childPid == 0)
  {
    /*
     * Place the child in its own process group.
     *
     * Its PID becomes its process-group ID.
     */
    if (setpgid(0, 0) == -1)
    {
      perror("setpgid");
      _exit(EXIT_FAILURE);
    }

    /*
     * External programs should use normal signal behaviour.
     */
    restoreDefaultSignalHandlers();

    SavedFileDescriptors unusedDescriptors;

    if (!applyRedirections(
            redirection,
            false,
            unusedDescriptors))
    {
      _exit(EXIT_FAILURE);
    }

    /*
     * Replace the child process with the requested external
     * program.
     */
    execvp(arguments[0], arguments);

    /*
     * execvp() returns only when execution fails.
     */
    perror(arguments[0]);
    _exit(EXIT_FAILURE);
  }

  /*
   * The parent also places the child in its own process group.
   * This prevents a timing race with the child.
   */
  if (setpgid(childPid, childPid) == -1)
  {
    if (errno != EACCES && errno != ESRCH)
    {
      perror("setpgid");
    }
  }

  /*
   * Background commands do not receive terminal control.
   */
  if (background)
  {
    printf(
        "[%d]\n",
        static_cast<int>(childPid));

    return 0;
  }

  /*
   * Record this process group as the foreground job.
   */
  setForegroundProcessGroup(childPid);

  /*
   * Give keyboard and terminal control to the foreground
   * program. This is required by interactive programs such
   * as vi and emacs.
   */
  giveTerminalTo(childPid);

  int status;

  while (waitpid(
             childPid,
             &status,
             WUNTRACED) == -1)
  {
    if (errno == EINTR)
    {
      continue;
    }

    perror("waitpid");

    /*
     * Ensure the custom shell gets terminal control back even
     * if waitpid() fails.
     */
    clearForegroundProcessGroup();
    takeTerminalBack();

    return -1;
  }

  /*
   * The foreground program has either completed or stopped.
   */
  clearForegroundProcessGroup();

  /*
   * Return keyboard and terminal control to the custom shell.
   */
  takeTerminalBack();

  if (WIFSTOPPED(status))
  {
    printf(
        "\nProcess %d stopped\n",
        static_cast<int>(childPid));
  }
  else if (WIFSIGNALED(status))
  {
    /*
     * Move the prompt to a new line after Ctrl+C.
     */
    printf("\n");
  }

  return 0;
}

void reapBackgroundProcesses()
{
  int status;
  pid_t completedPid;

  /*
   * WNOHANG prevents the shell from waiting when no background
   * process has completed.
   */
  while ((completedPid =
              waitpid(-1, &status, WNOHANG)) > 0)
  {
    printf(
        "\nBackground process %d finished\n",
        static_cast<int>(completedPid));
  }

  /*
   * ECHILD simply means there are no child processes waiting
   * to be collected.
   */
  if (completedPid == -1 && errno != ECHILD)
  {
    perror("waitpid");
  }
}