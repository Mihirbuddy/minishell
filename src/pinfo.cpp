#include "pinfo.hpp"
#include "shell.hpp"

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits.h>
#include <unistd.h>

#define PINFO_COMMAND_SIZE 4096
#define PINFO_STAT_SIZE 8192

static bool parsePid(
    const char *argument,
    pid_t &processId)
{
  if (argument == NULL || argument[0] == '\0')
  {
    return false;
  }

  char *invalidCharacter = NULL;

  errno = 0;

  long value = strtol(
      argument,
      &invalidCharacter,
      10);

  if (errno != 0 ||
      invalidCharacter == argument ||
      *invalidCharacter != '\0' ||
      value <= 0)
  {
    return false;
  }

  processId = static_cast<pid_t>(value);

  return true;
}

static bool readProcessStat(
    pid_t processId,
    char &processState,
    long &processGroupId,
    long &terminalForegroundGroupId,
    unsigned long &virtualMemory)
{
  char statPath[PATH_MAX];

  snprintf(
      statPath,
      sizeof(statPath),
      "/proc/%d/stat",
      static_cast<int>(processId));

  FILE *statFile = fopen(statPath, "r");

  if (statFile == NULL)
  {
    perror("pinfo");
    return false;
  }

  char statContent[PINFO_STAT_SIZE];

  if (fgets(
          statContent,
          sizeof(statContent),
          statFile) == NULL)
  {
    perror("pinfo");
    fclose(statFile);
    return false;
  }

  if (fclose(statFile) == EOF)
  {
    perror("pinfo");
    return false;
  }

  /*
   * The process name is enclosed in parentheses and may contain
   * spaces. Find the final ')' before reading the remaining fields.
   */
  char *closingParenthesis = strrchr(statContent, ')');

  if (closingParenthesis == NULL)
  {
    fprintf(stderr, "pinfo: invalid process information\n");
    return false;
  }

  long parentProcessId;
  long sessionId;
  long terminalNumber;
  unsigned long long startTime;

  int fieldsRead = sscanf(
      closingParenthesis + 1,

      /*
       * Fields after the process name:
       *
       * 3  state
       * 4  ppid
       * 5  pgrp
       * 6  session
       * 7  tty_nr
       * 8  tpgid
       * ...
       * 22 starttime
       * 23 vsize
       */
      " %c %ld %ld %ld %ld %ld"
      " %*u"
      " %*lu %*lu %*lu %*lu"
      " %*lu %*lu"
      " %*ld %*ld %*ld %*ld %*ld %*ld %*ld"
      " %llu %lu",

      &processState,
      &parentProcessId,
      &processGroupId,
      &sessionId,
      &terminalNumber,
      &terminalForegroundGroupId,
      &startTime,
      &virtualMemory);

  if (fieldsRead != 8)
  {
    fprintf(stderr, "pinfo: failed to parse process information\n");
    return false;
  }

  return true;
}

static bool readExecutablePath(
    pid_t processId,
    char *executablePath,
    size_t executablePathSize)
{
  char linkPath[PATH_MAX];

  snprintf(
      linkPath,
      sizeof(linkPath),
      "/proc/%d/exe",
      static_cast<int>(processId));

  ssize_t pathLength = readlink(
      linkPath,
      executablePath,
      executablePathSize - 1);

  if (pathLength == -1)
  {
    perror("pinfo");
    return false;
  }

  executablePath[pathLength] = '\0';

  return true;
}

static void printExecutablePath(
    const char *executablePath,
    const char *homeDirectory)
{
  size_t homeLength = strlen(homeDirectory);

  if (strcmp(executablePath, homeDirectory) == 0)
  {
    printf("~");
    return;
  }

  if (strncmp(
          executablePath,
          homeDirectory,
          homeLength) == 0 &&
      executablePath[homeLength] == '/')
  {
    printf("~%s", executablePath + homeLength);
    return;
  }

  printf("%s", executablePath);
}

int executePinfo(char *command, Shell &shell)
{
  if (command == NULL)
  {
    return -1;
  }

  char commandCopy[PINFO_COMMAND_SIZE];

  if (strlen(command) >= sizeof(commandCopy))
  {
    fprintf(stderr, "pinfo: command is too long\n");
    return -1;
  }

  strcpy(commandCopy, command);

  /*
   * Skip the command name.
   */
  char *commandName = strtok(commandCopy, " \t");

  if (commandName == NULL)
  {
    return -1;
  }

  char *pidArgument = strtok(NULL, " \t");
  char *extraArgument = strtok(NULL, " \t");

  if (extraArgument != NULL)
  {
    fprintf(stderr, "pinfo: Invalid arguments\n");
    return -1;
  }

  pid_t processId;

  /*
   * Without an argument, print information about the custom shell.
   */
  if (pidArgument == NULL)
  {
    processId = getpid();
  }
  else if (!parsePid(pidArgument, processId))
  {
    fprintf(stderr, "pinfo: Invalid process ID\n");
    return -1;
  }

  char processState;
  long processGroupId;
  long terminalForegroundGroupId;
  unsigned long virtualMemory;

  if (!readProcessStat(
          processId,
          processState,
          processGroupId,
          terminalForegroundGroupId,
          virtualMemory))
  {
    return -1;
  }

  char executablePath[PATH_MAX];

  if (!readExecutablePath(
          processId,
          executablePath,
          sizeof(executablePath)))
  {
    return -1;
  }

  printf(
      "Process Status -- %c",
      processState);

  /*
   * Add '+' when the process belongs to the terminal's current
   * foreground process group.
   */
  if (processGroupId > 0 &&
      processGroupId == terminalForegroundGroupId)
  {
    printf("+");
  }

  printf("\n");

  printf(
      "memory -- %lu {Virtual Memory}\n",
      virtualMemory);

  printf("Executable Path -- ");

  printExecutablePath(
      executablePath,
      shell.getHomeDirectory());

  printf("\n");

  return 0;
}