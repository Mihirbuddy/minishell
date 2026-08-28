#include "history.hpp"

#include <cstdio>
#include <cstring>
#include <limits.h>

#define MAX_HISTORY_COMMANDS 20
#define MAX_HISTORY_COMMAND_LENGTH 4096

static char historyCommands
    [MAX_HISTORY_COMMANDS]
    [MAX_HISTORY_COMMAND_LENGTH];

static int historyCount = 0;

static char historyFilePath[PATH_MAX];

static char *trimHistoryWhitespace(char *text)
{
  if (text == NULL)
  {
    return NULL;
  }

  while (*text == ' ' || *text == '\t')
  {
    text++;
  }

  if (*text == '\0')
  {
    return text;
  }

  char *end = text + strlen(text) - 1;

  while (end >= text &&
         (*end == ' ' || *end == '\t' ||
          *end == '\n' || *end == '\r'))
  {
    *end = '\0';
    end--;
  }

  return text;
}

static void storeCommandInMemory(const char *command)
{
  if (command == NULL || command[0] == '\0')
  {
    return;
  }

  /*
   * If 20 commands are already stored, remove the oldest by
   * shifting every command one position toward the beginning.
   */
  if (historyCount == MAX_HISTORY_COMMANDS)
  {
    for (int index = 1;
         index < MAX_HISTORY_COMMANDS;
         index++)
    {
      strcpy(
          historyCommands[index - 1],
          historyCommands[index]);
    }

    historyCount--;
  }

  strncpy(
      historyCommands[historyCount],
      command,
      MAX_HISTORY_COMMAND_LENGTH - 1);

  historyCommands[historyCount]
                 [MAX_HISTORY_COMMAND_LENGTH - 1] = '\0';

  historyCount++;
}

static bool saveHistoryToFile()
{
  FILE *historyFile = fopen(historyFilePath, "w");

  if (historyFile == NULL)
  {
    perror("history");
    return false;
  }

  for (int index = 0; index < historyCount; index++)
  {
    if (fprintf(
            historyFile,
            "%s\n",
            historyCommands[index]) < 0)
    {
      perror("history");
      fclose(historyFile);
      return false;
    }
  }

  if (fclose(historyFile) == EOF)
  {
    perror("history");
    return false;
  }

  return true;
}

bool initializeHistory(const char *homeDirectory)
{
  if (homeDirectory == NULL)
  {
    return false;
  }

  const char *historyFilename = "/.mihirshell_history";

  if (strlen(homeDirectory) +
          strlen(historyFilename) >=
      sizeof(historyFilePath))
  {
    fprintf(stderr, "History file path is too long\n");
    return false;
  }

  strcpy(historyFilePath, homeDirectory);
  strcat(historyFilePath, historyFilename);

  historyCount = 0;

  FILE *historyFile = fopen(historyFilePath, "r");

  if (historyFile == NULL)
  {
    /*
     * The history file does not exist during the first shell
     * session. It will be created after the first command.
     */
    return true;
  }

  char line[MAX_HISTORY_COMMAND_LENGTH];

  while (fgets(line, sizeof(line), historyFile) != NULL)
  {
    line[strcspn(line, "\r\n")] = '\0';

    char *cleanedLine = trimHistoryWhitespace(line);

    if (cleanedLine != NULL &&
        cleanedLine[0] != '\0')
    {
      storeCommandInMemory(cleanedLine);
    }
  }

  if (ferror(historyFile))
  {
    perror("history");
    fclose(historyFile);
    return false;
  }

  if (fclose(historyFile) == EOF)
  {
    perror("history");
    return false;
  }

  return true;
}

bool addHistoryCommand(const char *command)
{
  if (command == NULL)
  {
    return false;
  }

  char commandCopy[MAX_HISTORY_COMMAND_LENGTH];

  if (strlen(command) >= sizeof(commandCopy))
  {
    fprintf(stderr, "History command is too long\n");
    return false;
  }

  strcpy(commandCopy, command);

  char *cleanedCommand =
      trimHistoryWhitespace(commandCopy);

  if (cleanedCommand == NULL ||
      cleanedCommand[0] == '\0')
  {
    return true;
  }

  storeCommandInMemory(cleanedCommand);

  return saveHistoryToFile();
}

void printHistory(int numberOfCommands)
{
  if (numberOfCommands <= 0 || historyCount == 0)
  {
    return;
  }

  if (numberOfCommands > historyCount)
  {
    numberOfCommands = historyCount;
  }

  int startingIndex =
      historyCount - numberOfCommands;

  for (int index = startingIndex;
       index < historyCount;
       index++)
  {
    printf("%s\n", historyCommands[index]);
  }
}

int getHistoryCount()
{
  return historyCount;
}

const char *getHistoryCommand(int index)
{
  if (index < 0 || index >= historyCount)
  {
    return NULL;
  }

  return historyCommands[index];
}