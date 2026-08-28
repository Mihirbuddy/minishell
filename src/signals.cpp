#include "signals.hpp"

#include <cerrno>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <unistd.h>

/*
 * sig_atomic_t can be safely read and written from a signal handler.
 *
 * -1 means that no foreground command is currently running.
 */
static volatile sig_atomic_t foregroundProcessGroupId = -1;

static void handleForegroundSignal(int signalNumber)
{
  if (foregroundProcessGroupId <= 0)
  {
    /*
     * No foreground command is running.
     *
     * Ctrl+C and Ctrl+Z must not affect the shell.
     */
    return;
  }

  /*
   * A negative PID tells kill() to send the signal to every
   * process in the corresponding process group.
   *
   * This is important for pipelines.
   */
  kill(
      -static_cast<pid_t>(foregroundProcessGroupId),
      signalNumber);
}

bool initializeSignalHandlers()
{
  struct sigaction action;

  memset(&action, 0, sizeof(action));

  action.sa_handler = handleForegroundSignal;

  /*
   * Block signals while the handler is running.
   */
  if (sigemptyset(&action.sa_mask) == -1)
  {
    perror("sigemptyset");
    return false;
  }

  /*
   * Restart interrupted calls such as fgets() when Ctrl+C or
   * Ctrl+Z is pressed while no foreground command exists.
   */
  action.sa_flags = SA_RESTART;

  if (sigaction(SIGINT, &action, NULL) == -1)
  {
    perror("sigaction");
    return false;
  }

  if (sigaction(SIGTSTP, &action, NULL) == -1)
  {
    perror("sigaction");
    return false;
  }

  return true;
}

void restoreDefaultSignalHandlers()
{
  struct sigaction defaultAction;

  memset(&defaultAction, 0, sizeof(defaultAction));

  defaultAction.sa_handler = SIG_DFL;
  sigemptyset(&defaultAction.sa_mask);
  defaultAction.sa_flags = 0;

  /*
   * Child processes should respond normally to Ctrl+C and Ctrl+Z.
   */
  sigaction(SIGINT, &defaultAction, NULL);
  sigaction(SIGTSTP, &defaultAction, NULL);
}

void setForegroundProcessGroup(pid_t processGroupId)
{
  foregroundProcessGroupId =
      static_cast<sig_atomic_t>(processGroupId);
}

void clearForegroundProcessGroup()
{
  foregroundProcessGroupId = -1;
}