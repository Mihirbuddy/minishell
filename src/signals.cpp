#include "signals.hpp"

#include <csignal>
#include <cstdio>
#include <cstring>
#include <unistd.h>
#include <sys/wait.h>

/*
 * Process group currently executing in the foreground.
 *
 * -1 means no foreground command is currently running.
 */
static volatile sig_atomic_t foregroundProcessGroupId = -1;

/*
 * Process-group ID of our custom shell.
 *
 * We need this to return terminal control to the shell after
 * the foreground command finishes or stops.
 */
static pid_t shellProcessGroupId = -1;

static void handleForegroundSignal(int signalNumber)
{
  /*
   * Move to the next line after Ctrl+C or Ctrl+Z.
   */
  const char newline = '\n';
  write(STDOUT_FILENO, &newline, 1);

  /*
   * If no foreground command is running, do nothing.
   */
  if (foregroundProcessGroupId <= 0)
  {
    return;
  }

  /*
   * A negative process-group ID sends the signal to every
   * process in that process group.
   */
  kill(
      -static_cast<pid_t>(foregroundProcessGroupId),
      signalNumber);
}

static void handleChildSignal(int signalNumber)
{
  (void)signalNumber;

  /*
   * Do not collect children while a foreground command is running.
   * Its status must be collected by the foreground waitpid().
   */
  if (foregroundProcessGroupId > 0)
  {
    return;
  }

  /*
   * Collect every completed background child without blocking.
   */
  while (waitpid(-1, NULL, WNOHANG) > 0)
  {
  }
}
bool initializeSignalHandlers()
{
  /*
   * Save the custom shell's process-group ID.
   */
  shellProcessGroupId = getpgrp();

  /*
   * Configure Ctrl+C and Ctrl+Z.
   */
  struct sigaction action;

  memset(&action, 0, sizeof(action));

  action.sa_handler = handleForegroundSignal;

  if (sigemptyset(&action.sa_mask) == -1)
  {
    perror("sigemptyset");
    return false;
  }

  action.sa_flags = 0;

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

  /*
   * Ignore SIGTTOU so the shell can take terminal control back
   * after a foreground program finishes.
   */
  struct sigaction ignoreAction;

  memset(&ignoreAction, 0, sizeof(ignoreAction));

  ignoreAction.sa_handler = SIG_IGN;

  if (sigemptyset(&ignoreAction.sa_mask) == -1)
  {
    perror("sigemptyset");
    return false;
  }

  ignoreAction.sa_flags = 0;

  if (sigaction(SIGTTOU, &ignoreAction, NULL) == -1)
  {
    perror("sigaction");
    return false;
  }

  /*
   * Configure SIGCHLD for child-process state changes.
   */
  struct sigaction childAction;

  memset(&childAction, 0, sizeof(childAction));

  childAction.sa_handler = handleChildSignal;

  if (sigemptyset(&childAction.sa_mask) == -1)
  {
    perror("sigemptyset");
    return false;
  }

  /*
   * SA_RESTART prevents a completed background process from
   * destroying the command currently being typed.
   *
   * SA_NOCLDSTOP prevents SIGCHLD when Ctrl+Z merely stops a
   * process. We only need notification when a child terminates.
   */
  childAction.sa_flags = SA_RESTART | SA_NOCLDSTOP;

  if (sigaction(SIGCHLD, &childAction, NULL) == -1)
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
   * Child processes should use normal signal behaviour.
   */
  sigaction(SIGINT, &defaultAction, NULL);
  sigaction(SIGTSTP, &defaultAction, NULL);
  sigaction(SIGTTOU, &defaultAction, NULL);
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

void giveTerminalTo(pid_t processGroupId)
{
  /*
   * Do nothing when input is not coming from an interactive
   * terminal.
   */
  if (!isatty(STDIN_FILENO))
  {
    return;
  }

  /*
   * Make the external command or pipeline the terminal's
   * foreground process group.
   */
  if (tcsetpgrp(
          STDIN_FILENO,
          processGroupId) == -1)
  {
    perror("tcsetpgrp");
  }
}

void takeTerminalBack()
{
  if (!isatty(STDIN_FILENO))
  {
    return;
  }

  /*
   * Make the custom shell the terminal's foreground process
   * group again.
   */
  if (tcsetpgrp(
          STDIN_FILENO,
          shellProcessGroupId) == -1)
  {
    perror("tcsetpgrp");
  }
}