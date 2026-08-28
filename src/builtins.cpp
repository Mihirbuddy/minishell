#include "builtins.hpp"
#include "history.hpp"
#include "parser.hpp"
#include "shell.hpp"
#include "ls.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits.h>
#include <unistd.h>

#define COMMAND_BUFFER_SIZE 4096
#define ARGUMENT_BUFFER_SIZE 4096

static const char *BUILTIN_COMMANDS[] = {
    "cd",
    "pwd",
    "echo",
    "ls",
    "history",
    "exit"};

static const int BUILTIN_COMMAND_COUNT =
    sizeof(BUILTIN_COMMANDS) /
    sizeof(BUILTIN_COMMANDS[0]);

static int executePwd()
{
  char currentDirectory[PATH_MAX];

  if (getcwd(currentDirectory, sizeof(currentDirectory)) == NULL)
  {
    perror("pwd");
    return -1;
  }

  printf("%s\n", currentDirectory);

  return 0;
}

static void removeOuterQuotes(char *text)
{
  if (text == NULL)
  {
    return;
  }

  size_t length = strlen(text);

  if (length < 2)
  {
    return;
  }

  bool doubleQuoted =
      text[0] == '"' && text[length - 1] == '"';

  bool singleQuoted =
      text[0] == '\'' && text[length - 1] == '\'';

  if (!doubleQuoted && !singleQuoted)
  {
    return;
  }

  /*
   * Shift everything one position to the left so that the
   * opening quote is removed.
   *
   * length - 2 characters belong to the actual content.
   */
  memmove(text, text + 1, length - 2);

  /*
   * Place '\0' immediately after the content.
   */
  text[length - 2] = '\0';
}

static int executeHistory(char *command)
{
  if (command == NULL)
  {
    return -1;
  }

  char commandCopy[COMMAND_BUFFER_SIZE];

  if (strlen(command) >= sizeof(commandCopy))
  {
    fprintf(stderr, "history: command is too long\n");
    return -1;
  }

  strcpy(commandCopy, command);

  /*
   * Skip the word "history".
   */
  char *commandName = strtok(commandCopy, " \t");

  if (commandName == NULL)
  {
    return -1;
  }

  char *numberArgument = strtok(NULL, " \t");
  char *extraArgument = strtok(NULL, " \t");

  if (extraArgument != NULL)
  {
    fprintf(stderr, "history: Invalid arguments\n");
    return -1;
  }

  /*
   * history without an argument displays at most 10 commands.
   */
  if (numberArgument == NULL)
  {
    printHistory(10);
    return 0;
  }

  char *invalidCharacter = NULL;

  long requestedCount = strtol(
      numberArgument,
      &invalidCharacter,
      10);

  if (invalidCharacter == numberArgument ||
      *invalidCharacter != '\0' ||
      requestedCount <= 0 ||
      requestedCount > 20)
  {
    fprintf(
        stderr,
        "history: count must be between 1 and 20\n");

    return -1;
  }

  printHistory(static_cast<int>(requestedCount));

  return 0;
}

static int executeEcho(char *command)
{
  if (command == NULL)
  {
    printf("\n");
    return 0;
  }

  /*
   * Move past the command name "echo".
   */
  char *arguments = command + 4;

  /*
   * Skip the spaces and tabs separating echo from its arguments.
   */
  while (*arguments == ' ' || *arguments == '\t')
  {
    arguments++;
  }

  /*
   * echo without arguments prints only a newline.
   */
  if (*arguments == '\0')
  {
    printf("\n");
    return 0;
  }

  char output[ARGUMENT_BUFFER_SIZE];

  if (strlen(arguments) >= sizeof(output))
  {
    fprintf(stderr, "echo: arguments are too long\n");
    return -1;
  }

  strcpy(output, arguments);

  /*
   * Remove trailing spaces and tabs.
   */
  char *cleanedOutput = trimWhitespace(output);

  removeOuterQuotes(cleanedOutput);

  printf("%s\n", cleanedOutput);

  return 0;
}

static int countCdArguments(char *arguments,
                            char *firstArgument,
                            size_t firstArgumentSize)
{
  int argumentCount = 0;
  char *current = arguments;

  firstArgument[0] = '\0';

  while (current != NULL && *current != '\0')
  {
    /*
     * Skip spaces and tabs before the next argument.
     */
    while (*current == ' ' || *current == '\t')
    {
      current++;
    }

    if (*current == '\0')
    {
      break;
    }

    char *argumentStart = current;

    /*
     * Move until the next space, tab, or end of string.
     */
    while (*current != '\0' &&
           *current != ' ' &&
           *current != '\t')
    {
      current++;
    }

    size_t argumentLength =
        static_cast<size_t>(current - argumentStart);

    argumentCount++;

    /*
     * We only need to save the first argument.
     */
    if (argumentCount == 1)
    {
      if (argumentLength >= firstArgumentSize)
      {
        fprintf(stderr, "cd: path is too long\n");
        return -1;
      }

      memcpy(firstArgument, argumentStart, argumentLength);
      firstArgument[argumentLength] = '\0';
    }
  }

  return argumentCount;
}

static bool createCdDestination(const char *argument,
                                Shell &shell,
                                char *destination,
                                size_t destinationSize)
{
  if (argument == NULL || argument[0] == '\0')
  {
    if (strlen(shell.getHomeDirectory()) >= destinationSize)
    {
      fprintf(stderr, "cd: path is too long\n");
      return false;
    }

    strcpy(destination, shell.getHomeDirectory());
    return true;
  }

  /*
   * cd ~
   */
  if (strcmp(argument, "~") == 0)
  {
    if (strlen(shell.getHomeDirectory()) >= destinationSize)
    {
      fprintf(stderr, "cd: path is too long\n");
      return false;
    }

    strcpy(destination, shell.getHomeDirectory());
    return true;
  }

  /*
   * cd -
   */
  if (strcmp(argument, "-") == 0)
  {
    if (strlen(shell.getPreviousDirectory()) >= destinationSize)
    {
      fprintf(stderr, "cd: path is too long\n");
      return false;
    }

    strcpy(destination, shell.getPreviousDirectory());
    return true;
  }

  /*
   * Support paths such as:
   *
   * cd ~/directory
   */
  if (argument[0] == '~' && argument[1] == '/')
  {
    size_t homeLength = strlen(shell.getHomeDirectory());
    size_t remainingLength = strlen(argument + 1);

    if (homeLength + remainingLength >= destinationSize)
    {
      fprintf(stderr, "cd: path is too long\n");
      return false;
    }

    strcpy(destination, shell.getHomeDirectory());
    strcat(destination, argument + 1);

    return true;
  }

  /*
   * Ordinary paths, including "." and "..", can be passed
   * directly to chdir().
   */
  if (strlen(argument) >= destinationSize)
  {
    fprintf(stderr, "cd: path is too long\n");
    return false;
  }

  strcpy(destination, argument);

  return true;
}

static int executeCd(char *command, Shell &shell)
{
  char currentDirectory[PATH_MAX];
  char destination[PATH_MAX];
  char firstArgument[PATH_MAX];

  if (getcwd(currentDirectory, sizeof(currentDirectory)) == NULL)
  {
    perror("cd");
    return -1;
  }

  /*
   * Move past "cd".
   */
  char *arguments = command + 2;

  int argumentCount = countCdArguments(
      arguments,
      firstArgument,
      sizeof(firstArgument));

  if (argumentCount == -1)
  {
    return -1;
  }

  if (argumentCount > 1)
  {
    fprintf(stderr, "Invalid arguments\n");
    return -1;
  }

  const char *argument = NULL;

  if (argumentCount == 1)
  {
    argument = firstArgument;
  }

  if (!createCdDestination(
          argument,
          shell,
          destination,
          sizeof(destination)))
  {
    return -1;
  }

  if (chdir(destination) == -1)
  {
    perror("cd");
    return -1;
  }

  /*
   * Update the previous directory only after chdir()
   * succeeds.
   */
  if (!shell.setPreviousDirectory(currentDirectory))
  {
    /*
     * The directory has already changed, but storing the
     * previous directory failed.
     */
    fprintf(stderr, "cd: failed to update previous directory\n");
    return -1;
  }

  /*
   * Like a normal shell, cd - prints the directory it moved to.
   */
  if (argument != NULL && strcmp(argument, "-") == 0)
  {
    char newDirectory[PATH_MAX];

    if (getcwd(newDirectory, sizeof(newDirectory)) == NULL)
    {
      perror("cd");
      return -1;
    }

    printf("%s\n", newDirectory);
  }

  return 0;
}

bool isBuiltinCommand(const char *commandName)
{
  if (commandName == NULL)
  {
    return false;
  }

  for (int index = 0;
       index < BUILTIN_COMMAND_COUNT;
       index++)
  {
    if (strcmp(
            commandName,
            BUILTIN_COMMANDS[index]) == 0)
    {
      return true;
    }
  }

  return false;
}

int getBuiltinCommandCount()
{
  return BUILTIN_COMMAND_COUNT;
}

const char *getBuiltinCommandName(int index)
{
  if (index < 0 || index >= BUILTIN_COMMAND_COUNT)
  {
    return NULL;
  }

  return BUILTIN_COMMANDS[index];
}

int executeBuiltinCommand(char *command, Shell &shell)
{
  if (command == NULL)
  {
    return -1;
  }

  char commandCopy[COMMAND_BUFFER_SIZE];

  if (strlen(command) >= sizeof(commandCopy))
  {
    fprintf(stderr, "Command is too long\n");
    return -1;
  }

  strcpy(commandCopy, command);

  /*
   * Extract only the command name.
   */
  char *commandName = strtok(commandCopy, " \t");

  if (commandName == NULL)
  {
    return 0;
  }

  if (strcmp(commandName, "exit") == 0)
  {
    /*
     * For now, exit must not have additional arguments.
     */
    char *extraArgument = strtok(NULL, " \t");

    if (extraArgument != NULL)
    {
      fprintf(stderr, "exit: Invalid arguments\n");
      return -1;
    }

    shell.stop();
    return 0;
  }

  if (strcmp(commandName, "pwd") == 0)
  {
    char *extraArgument = strtok(NULL, " \t");

    if (extraArgument != NULL)
    {
      fprintf(stderr, "pwd: Invalid arguments\n");
      return -1;
    }

    return executePwd();
  }

  if (strcmp(commandName, "echo") == 0)
  {
    return executeEcho(command);
  }

  if (strcmp(commandName, "history") == 0)
  {
    return executeHistory(command);
  }

  if (strcmp(commandName, "ls") == 0)
  {
    return executeLs(command, shell);
  }

  if (strcmp(commandName, "cd") == 0)
  {
    return executeCd(command, shell);
  }

  return -1;
}