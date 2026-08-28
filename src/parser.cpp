#include "parser.hpp"
#include "builtins.hpp"
#include "executor.hpp"
#include "shell.hpp"

#include "redirection.hpp"
#include "pipeline.hpp"

#include <cstdio>
#include <cstring>

#define MAX_COMMANDS_PER_LINE 128
#define MAX_COMMAND_LENGTH 4096

static bool isWhitespace(char character)
{
  return character == ' ' || character == '\t';
}

char *trimWhitespace(char *text)
{
  if (text == NULL)
  {
    return NULL;
  }

  /*
   * Skip leading spaces and tabs.
   */
  while (*text != '\0' && isWhitespace(*text))
  {
    text++;
  }

  /*
   * The complete string contained only whitespace.
   */
  if (*text == '\0')
  {
    return text;
  }

  char *end = text + strlen(text) - 1;

  /*
   * Remove trailing spaces and tabs.
   */
  while (end >= text && isWhitespace(*end))
  {
    *end = '\0';
    end--;
  }

  return text;
}

static bool getCommandName(const char *command,
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

  if (*current == '\0')
  {
    commandName[0] = '\0';
    return false;
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

  if (length >= commandNameSize)
  {
    fprintf(stderr, "Command name is too long\n");
    commandName[0] = '\0';
    return false;
  }

  memcpy(commandName, commandStart, length);
  commandName[length] = '\0';

  return true;
}

static bool extractBackgroundSymbol(char *command)
{
  if (command == NULL)
  {
    return false;
  }

  char *ampersand = strchr(command, '&');

  /*
   * No '&' means this is a foreground command.
   */
  if (ampersand == NULL)
  {
    return false;
  }

  /*
   * Ensure that no second '&' exists.
   */
  if (strchr(ampersand + 1, '&') != NULL)
  {
    fprintf(stderr, "Invalid use of background operator\n");
    return false;
  }

  /*
   * Everything after '&' must contain only whitespace.
   *
   * Valid:
   *     sleep 5 &
   *
   * Invalid:
   *     sleep & 5
   */
  char *current = ampersand + 1;

  while (*current != '\0')
  {
    if (!isWhitespace(*current))
    {
      fprintf(
          stderr,
          "Background operator must appear at the end\n");
      return false;
    }

    current++;
  }

  /*
   * Remove '&' from the command.
   */
  *ampersand = '\0';

  /*
   * Remove spaces appearing immediately before '&'.
   */
  trimWhitespace(command);

  return true;
}

static bool containsInvalidAmpersand(const char *command)
{
  if (command == NULL)
  {
    return false;
  }

  const char *ampersand = strchr(command, '&');

  if (ampersand == NULL)
  {
    return false;
  }

  /*
   * After '&', only spaces and tabs are valid.
   */
  const char *current = ampersand + 1;

  while (*current != '\0')
  {
    if (!isWhitespace(*current))
    {
      return true;
    }

    current++;
  }

  /*
   * More than one '&' is invalid.
   */

  // this condition is unnecesssary
  if (strchr(ampersand + 1, '&') != NULL)
  {
    return true;
  }

  return false;
}

static void processSingleCommand(char* command, Shell& shell) {
    char* cleanedCommand = trimWhitespace(command);

    if (cleanedCommand == NULL || cleanedCommand[0] == '\0') {
        return;
    }

    if (containsInvalidAmpersand(cleanedCommand)) {
        fprintf(stderr, "Invalid use of background operator\n");
        return;
    }

    bool background = extractBackgroundSymbol(cleanedCommand);

    cleanedCommand = trimWhitespace(cleanedCommand);

    if (cleanedCommand == NULL || cleanedCommand[0] == '\0') {
        fprintf(stderr, "Invalid command before '&'\n");
        return;
    }

    /*
     * Pipelines have their own execution path because every command
     * in the pipeline must run in a separate child process.
     */
    if (strchr(cleanedCommand, '|') != NULL) {
        executePipeline(cleanedCommand, background, shell);
        return;
    }

    char executableCommand[MAX_COMMAND_LENGTH];
    RedirectionInfo redirection;

    if (!parseRedirections(
            cleanedCommand,
            executableCommand,
            sizeof(executableCommand),
            redirection)) {
        return;
    }

    char commandName[MAX_COMMAND_LENGTH];

    if (!getCommandName(
            executableCommand,
            commandName,
            sizeof(commandName))) {
        return;
    }

    if (isBuiltinCommand(commandName)) {
        SavedFileDescriptors savedDescriptors;

        if (!applyRedirections(
                redirection,
                true,
                savedDescriptors)) {
            return;
        }

        executeBuiltinCommand(executableCommand, shell);

        fflush(stdout);
        restoreRedirections(savedDescriptors);

        return;
    }

    executeExternalCommand(
        executableCommand,
        background,
        redirection
    );
}

void parseInput(char *input, Shell &shell)
{
  if (input == NULL)
  {
    return;
  }

  /*
   * Store copies of all semicolon-separated commands before
   * executing them.
   *
   * Command execution uses strtok(), so completing semicolon
   * parsing first prevents nested strtok() calls from interfering
   * with each other.
   */
  char commands[MAX_COMMANDS_PER_LINE][MAX_COMMAND_LENGTH];

  int commandCount = 0;

  char *command = strtok(input, ";");

  while (command != NULL)
  {
    char *cleanedCommand = trimWhitespace(command);

    if (cleanedCommand != NULL &&
        cleanedCommand[0] != '\0')
    {
      if (commandCount >= MAX_COMMANDS_PER_LINE)
      {
        fprintf(
            stderr,
            "Too many commands in a single line\n");
        break;
      }

      if (strlen(cleanedCommand) >= MAX_COMMAND_LENGTH)
      {
        fprintf(stderr, "Command is too long\n");
      }
      else
      {
        strcpy(commands[commandCount], cleanedCommand);
        commandCount++;
      }
    }

    command = strtok(NULL, ";");
  }

  for (int index = 0;
       index < commandCount && shell.isRunning();
       index++)
  {
    processSingleCommand(commands[index], shell);
  }
}