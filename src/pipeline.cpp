#include "pipeline.hpp"
#include "signals.hpp"

#include "builtins.hpp"
#include "redirection.hpp"
#include "shell.hpp"

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define MAX_PIPELINE_COMMANDS 32
#define MAX_PIPELINE_COMMAND_LENGTH 4096
#define MAX_PIPELINE_ARGUMENTS 256

static bool isWhitespace(char character)
{
  return character == ' ' || character == '\t';
}

static char *trimPipelineWhitespace(char *text)
{
  if (text == NULL)
  {
    return NULL;
  }

  while (*text != '\0' && isWhitespace(*text))
  {
    text++;
  }

  if (*text == '\0')
  {
    return text;
  }

  char *end = text + strlen(text) - 1;

  while (end >= text && isWhitespace(*end))
  {
    *end = '\0';
    end--;
  }

  return text;
}

static int splitPipeline(
    const char *command,
    char pipelineCommands[][MAX_PIPELINE_COMMAND_LENGTH])
{
  if (command == NULL)
  {
    return -1;
  }

  int commandCount = 0;
  size_t commandStart = 0;
  size_t currentIndex = 0;
  size_t commandLength = strlen(command);

  while (currentIndex <= commandLength)
  {
    if (command[currentIndex] == '|' ||
        command[currentIndex] == '\0')
    {
      size_t length = currentIndex - commandStart;

      if (length >= MAX_PIPELINE_COMMAND_LENGTH)
      {
        fprintf(stderr, "Pipeline command is too long\n");
        return -1;
      }

      if (commandCount >= MAX_PIPELINE_COMMANDS)
      {
        fprintf(stderr, "Too many pipeline commands\n");
        return -1;
      }

      memcpy(
          pipelineCommands[commandCount],
          command + commandStart,
          length);

      pipelineCommands[commandCount][length] = '\0';

      char *cleanedCommand = trimPipelineWhitespace(
          pipelineCommands[commandCount]);

      /*
       * Reject:
       *
       * | ls
       * ls |
       * ls || wc
       */
      if (cleanedCommand == NULL ||
          cleanedCommand[0] == '\0')
      {
        fprintf(stderr, "Invalid null command in pipeline\n");
        return -1;
      }

      /*
       * trimPipelineWhitespace() may return a pointer inside
       * the array. Move the cleaned command to its beginning.
       */
      if (cleanedCommand != pipelineCommands[commandCount])
      {
        memmove(
            pipelineCommands[commandCount],
            cleanedCommand,
            strlen(cleanedCommand) + 1);
      }

      commandCount++;
      commandStart = currentIndex + 1;
    }

    currentIndex++;
  }

  return commandCount;
}

static bool getPipelineCommandName(
    const char *command,
    char *commandName,
    size_t commandNameSize)
{
  if (command == NULL ||
      commandName == NULL ||
      commandNameSize == 0)
  {
    return false;
  }

  const char *current = command;

  while (*current == ' ' || *current == '\t')
  {
    current++;
  }

  const char *commandStart = current;

  while (*current != '\0' &&
         *current != ' ' &&
         *current != '\t')
  {
    current++;
  }

  size_t length =
      static_cast<size_t>(current - commandStart);

  if (length == 0 || length >= commandNameSize)
  {
    return false;
  }

  memcpy(commandName, commandStart, length);
  commandName[length] = '\0';

  return true;
}

static int createPipelineArgumentArray(
    char *command,
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

static void executePipelineCommand(
    char *command,
    Shell &shell)
{
  char commandName[MAX_PIPELINE_COMMAND_LENGTH];

  if (!getPipelineCommandName(
          command,
          commandName,
          sizeof(commandName)))
  {
    fprintf(stderr, "Invalid pipeline command\n");
    _exit(EXIT_FAILURE);
  }

  /*
   * Built-ins in pipelines execute inside the child.
   *
   * For example:
   *
   * pwd | wc -c
   *
   * Our custom pwd runs in the child and sends its output through
   * the pipe.
   */
  if (isBuiltinCommand(commandName))
  {
    int result = executeBuiltinCommand(command, shell);

    /*
     * Built-ins use printf(), so flush their output before
     * terminating the child with _exit().
     */
    fflush(stdout);

    _exit(result == 0 ? EXIT_SUCCESS : EXIT_FAILURE);
  }

  char *arguments[MAX_PIPELINE_ARGUMENTS];

  int argumentCount = createPipelineArgumentArray(
      command,
      arguments,
      MAX_PIPELINE_ARGUMENTS);

  if (argumentCount <= 0)
  {
    _exit(EXIT_FAILURE);
  }

  execvp(arguments[0], arguments);

  perror(arguments[0]);
  _exit(EXIT_FAILURE);
}

static void closeAllPipes(
    int pipeDescriptors[][2],
    int pipeCount)
{
  for (int index = 0; index < pipeCount; index++)
  {
    close(pipeDescriptors[index][0]);
    close(pipeDescriptors[index][1]);
  }
}

int executePipeline(
    char *command,
    bool background,
    Shell &shell)
{
  if (command == NULL)
  {
    return -1;
  }

  char pipelineCommands
      [MAX_PIPELINE_COMMANDS]
      [MAX_PIPELINE_COMMAND_LENGTH];

  int commandCount = splitPipeline(
      command,
      pipelineCommands);

  if (commandCount < 2)
  {
    fprintf(stderr, "A pipeline requires at least two commands\n");
    return -1;
  }

  /*
   * N commands require N - 1 pipes.
   */
  int pipeCount = commandCount - 1;
  int pipeDescriptors[MAX_PIPELINE_COMMANDS - 1][2];

  for (int index = 0; index < pipeCount; index++)
  {
    if (pipe(pipeDescriptors[index]) == -1)
    {
      perror("pipe");

      /*
       * Close pipes that were successfully created earlier.
       */
      for (int previous = 0; previous < index; previous++)
      {
        close(pipeDescriptors[previous][0]);
        close(pipeDescriptors[previous][1]);
      }

      return -1;
    }
  }

  pid_t childPids[MAX_PIPELINE_COMMANDS];
  int childrenCreated = 0;
  pid_t pipelineProcessGroupId = -1;

  for (int index = 0; index < commandCount; index++)
  {
    pid_t childPid = fork();

    if (childPid < 0)
    {
      perror("fork");
      closeAllPipes(pipeDescriptors, pipeCount);

      for (int childIndex = 0;
           childIndex < childrenCreated;
           childIndex++)
      {
        waitpid(childPids[childIndex], NULL, 0);
      }

      return -1;
    }

    if (childPid == 0)
    {

      pid_t childProcessGroup;

      if (index == 0)
      {
        /*
         * The first child's PID becomes the process-group ID.
         */
        childProcessGroup = 0;
      }
      else
      {
        /*
         * Later children join the first child's process group.
         */
        childProcessGroup = pipelineProcessGroupId;
      }

      if (setpgid(0, childProcessGroup) == -1)
      {
        perror("setpgid");
        _exit(EXIT_FAILURE);
      }

      restoreDefaultSignalHandlers();
      /*
       * Every command except the first reads from the previous
       * pipe.
       */

      if (index > 0)
      {
        if (dup2(
                pipeDescriptors[index - 1][0],
                STDIN_FILENO) == -1)
        {
          perror("dup2");
          _exit(EXIT_FAILURE);
        }
      }

      /*
       * Every command except the last writes into the next
       * pipe.
       */
      if (index < commandCount - 1)
      {
        if (dup2(
                pipeDescriptors[index][1],
                STDOUT_FILENO) == -1)
        {
          perror("dup2");
          _exit(EXIT_FAILURE);
        }
      }

      /*
       * After dup2(), the child no longer needs any original
       * pipe descriptors.
       */
      closeAllPipes(pipeDescriptors, pipeCount);

      char executableCommand[MAX_PIPELINE_COMMAND_LENGTH];
      RedirectionInfo redirection;

      if (!parseRedirections(
              pipelineCommands[index],
              executableCommand,
              sizeof(executableCommand),
              redirection))
      {
        _exit(EXIT_FAILURE);
      }

      SavedFileDescriptors unusedDescriptors;

      /*
       * Apply explicit redirection after connecting pipes.
       *
       * Therefore:
       *
       * echo hello > file.txt | wc
       *
       * redirects echo into file.txt instead of the pipe.
       */
      if (!applyRedirections(
              redirection,
              false,
              unusedDescriptors))
      {
        _exit(EXIT_FAILURE);
      }

      executePipelineCommand(executableCommand, shell);
    }

    if (index == 0)
    {
      pipelineProcessGroupId = childPid;
    }

    if (setpgid(childPid, pipelineProcessGroupId) == -1)
    {
      if (errno != EACCES && errno != ESRCH)
      {
        perror("setpgid");
      }
    }

    childPids[index] = childPid;
    childrenCreated++;
  }

  /*
   * The parent shell must close every pipe descriptor.
   *
   * Otherwise, readers may never receive EOF.
   */
  closeAllPipes(pipeDescriptors, pipeCount);

  if (background)
  {
    printf(
        "[%d]\n",
        static_cast<int>(childPids[0]));

    return 0;
  }

  /*
   * Wait for every process in the foreground pipeline.
   */

  setForegroundProcessGroup(pipelineProcessGroupId);
giveTerminalTo(pipelineProcessGroupId);

bool pipelineStopped = false;
  for (int index = 0; index < commandCount; index++)
  {
    int status;

    while (waitpid(
               childPids[index],
               &status,
               WUNTRACED) == -1)
    {
      if (errno == EINTR)
      {
        continue;
      }

      if (errno == ECHILD)
      {
        break;
      }

      perror("waitpid");
      clearForegroundProcessGroup();
      takeTerminalBack();
      return -1;
    }

    if (WIFSTOPPED(status))
    {
      pipelineStopped = true;
    }
  }

  clearForegroundProcessGroup();
takeTerminalBack();



  if (pipelineStopped)
  {
    printf(
        "Pipeline %d stopped\n",
        static_cast<int>(pipelineProcessGroupId));
  }

  return 0;
}