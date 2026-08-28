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

  /*
   * Separate the command using spaces and tabs.
   *
   * Example:
   *
   * ls -l src
   *
   * becomes:
   *
   * arguments[0] = "ls"
   * arguments[1] = "-l"
   * arguments[2] = "src"
   * arguments[3] = NULL
   */
  char *token = strtok(command, " \t");

  while (token != NULL)
  {
    /*
     * One position must remain available for NULL because
     * execvp() requires a NULL-terminated argument array.
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
     * Create a new process group.
     *
     * Because the group ID is 0, the child's own PID becomes
     * its process-group ID.
     */
    if (setpgid(0, 0) == -1)
    {
      perror("setpgid");
      _exit(EXIT_FAILURE);
    }

    /*
     * The shell handles Ctrl+C and Ctrl+Z specially, but the
     * external program must use their normal default behaviour.
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

    execvp(arguments[0], arguments);

    perror(arguments[0]);
    _exit(EXIT_FAILURE);
  }

  /*
   * The parent also calls setpgid() to avoid a race where the
   * child has not set its group before a signal arrives.
   */
  if (setpgid(childPid, childPid) == -1)
  {
    /*
     * EACCES can occur if the child executed execvp() before
     * the parent reached setpgid(). In that case, the child may
     * already have set its process group successfully.
     */
    if (errno != EACCES && errno != ESRCH)
    {
      perror("setpgid");
    }
  }

  if (background)
  {
    printf("[%d]\n", static_cast<int>(childPid));
    return 0;
  }

  /*
   * Ctrl+C and Ctrl+Z should now be forwarded to this group.
   */
  setForegroundProcessGroup(childPid);

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
    clearForegroundProcessGroup();
    return -1;
  }

  /*
   * The process either terminated or stopped, so it is no longer
   * the shell's foreground process.
   */
  clearForegroundProcessGroup();

  if (WIFSTOPPED(status))
  {
    printf(
        "Process %d stopped\n",
        static_cast<int>(childPid));
  }

  return 0;
}

void reapBackgroundProcesses()
{
  int status;

  /*status is one integer, but its different bits encode different pieces of information.
  The operating system uses some bits to record:

How the child terminated
The child’s exit code
The terminating signal, if any
Whether the child was stopped or continued

waitpid() writes this encoded value into status:
The macros decode the relevant bits.*/
  pid_t completedPid;

  /*
   * WNOHANG means:
   *
   * Return immediately if no background process has completed.
   *
   * Calling waitpid() repeatedly ensures that all completed
   * background children are reaped.
   */
  while ((completedPid = waitpid(-1, &status, WNOHANG)) > 0)
  {
    printf(
        "\nBackground process %d finished\n",
        static_cast<int>(completedPid));
  }

  /*
   * completedPid == 0:
   *     Child processes exist, but none have completed.
   *
   * completedPid == -1:
   *     No child is currently waiting to be reaped, or an error
   *     occurred.
   *
   * ECHILD is normal when the shell has no child processes.
   */
  if (completedPid == -1 && errno != ECHILD)
  {
    perror("waitpid");
  }
}