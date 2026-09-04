#include "search.hpp"

#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <limits.h>
#include <sys/stat.h>

static bool searchRecursively(
    const char *directoryPath,
    const char *targetName)
{
  DIR *directory = opendir(directoryPath);

  if (directory == NULL)
  {
    return false;
  }

  struct dirent *entry;

  while ((entry = readdir(directory)) != NULL)
  {
    if (strcmp(entry->d_name, ".") == 0 ||
        strcmp(entry->d_name, "..") == 0)
    {
      continue;
    }

    if (strcmp(entry->d_name, targetName) == 0)
    {
      closedir(directory);
      return true;
    }

    char completePath[PATH_MAX];

    int length = snprintf(
        completePath,
        sizeof(completePath),
        "%s/%s",
        directoryPath,
        entry->d_name);

    if (length < 0 ||
        static_cast<size_t>(length) >= sizeof(completePath))
    {
      continue;
    }

    struct stat fileInformation;

    /*
     * lstat() prevents following symbolic links and avoids
     * recursive symbolic-link loops.
     */
    if (lstat(completePath, &fileInformation) == -1)
    {
      continue;
    }

    if (S_ISDIR(fileInformation.st_mode))
    {
      if (searchRecursively(
              completePath,
              targetName))
      {
        closedir(directory);
        return true;
      }
    }
  }

  closedir(directory);
  return false;
}

int executeSearch(char *command)
{
  if (command == NULL)
  {
    return -1;
  }

  char commandCopy[4096];

  if (strlen(command) >= sizeof(commandCopy))
  {
    fprintf(stderr, "search: command is too long\n");
    return -1;
  }

  strcpy(commandCopy, command);

  strtok(commandCopy, " \t");

  char *targetName = strtok(NULL, " \t");
  char *extraArgument = strtok(NULL, " \t");

  if (targetName == NULL || extraArgument != NULL)
  {
    fprintf(stderr, "search: Invalid arguments\n");
    return -1;
  }

  if (searchRecursively(".", targetName))
  {
    printf("True\n");
  }
  else
  {
    printf("False\n");
  }

  return 0;
}