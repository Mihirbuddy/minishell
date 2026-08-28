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

  /*
  see initially jab fork run hua to ek child process bana and ek parent process, chidl ki jo bhi new process id banegi vo parent ke childpid hogi and chilp ki childpid 0 hogi ,
  now initally parent jo hai vo shell program chala raha hoga , so child jo banega usko bhi isi code ki ek copy milegi to run to child process bhi abhi shell hi execute kar raha hoga ,
  */

  if (childPid == 0)
  {
    /*
     * This block is executed by the child process.
     *
     * execvp() replaces the child process with the requested
     * external program.
     *
     * arguments[0] is the command name.
     *
     * The 'p' in execvp means that PATH is searched for the
     * executable.
     */

    /*now because child has childpid as 0 it has entered the loop and execvp will give it new task stored in arguments[0]
    see if execvp is successfull it does not return anything , it only returns when it fails
    */
    SavedFileDescriptors unusedDescriptors;

    if (!applyRedirections(
            redirection,
            false,
            unusedDescriptors))
    {
      _exit(EXIT_FAILURE);
    }

    execvp(arguments[0], arguments);

    /*now don't confuse that even if execvp succeded we are still writing the perror ,
    see ye jo bhi code hai vo child process me jaega hi nahi , like jaisi hi hamne execvp karke child process ko ls vala task diya , to ab usme ls ka code aagaya , ye code hai hi nahi , ab vo ls ka code hi execute kar raha hoga ,
    but agar vo fail hua  to it returns to parent which has this code only */
    perror(arguments[0]);

    /*
     * Use _exit() inside the child after fork().
     *
     * Unlike exit(), _exit() does not flush copied parent
     * I/O buffers or run parent cleanup handlers.
     */
    _exit(EXIT_FAILURE);
  }

  /*
   * This block is executed by the parent shell.
   */
  if (background)
  {
    printf("[%d]\n", static_cast<int>(childPid));
    return 0;
  }

  int status;

  /*
   * Wait only for the foreground child that we just created.
   */
  while (waitpid(childPid, &status, 0) == -1)
  {
    /*
     * waitpid() may be interrupted by a signal. In that case,
     * call it again.
     */
    if (errno == EINTR)
    {
      continue;
    }

    perror("waitpid");
    return -1;
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