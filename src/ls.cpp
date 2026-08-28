#include "ls.hpp"
#include "shell.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <dirent.h>
#include <grp.h>
#include <limits.h>
#include <pwd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define MAX_LS_ARGUMENTS 128

struct LsOptions
{
  bool showHidden;
  bool longFormat;
};

static void printPermissions(mode_t mode)
{
  char permissions[11];

  if (S_ISDIR(mode))
  {
    permissions[0] = 'd';
  }
  else if (S_ISLNK(mode))
  {
    permissions[0] = 'l';
  }
  else if (S_ISCHR(mode))
  {
    permissions[0] = 'c';
  }
  else if (S_ISBLK(mode))
  {
    permissions[0] = 'b';
  }
  else if (S_ISFIFO(mode))
  {
    permissions[0] = 'p';
  }
  else if (S_ISSOCK(mode))
  {
    permissions[0] = 's';
  }
  else
  {
    permissions[0] = '-';
  }

  permissions[1] = (mode & S_IRUSR) ? 'r' : '-';
  permissions[2] = (mode & S_IWUSR) ? 'w' : '-';
  permissions[3] = (mode & S_IXUSR) ? 'x' : '-';

  permissions[4] = (mode & S_IRGRP) ? 'r' : '-';
  permissions[5] = (mode & S_IWGRP) ? 'w' : '-';
  permissions[6] = (mode & S_IXGRP) ? 'x' : '-';

  permissions[7] = (mode & S_IROTH) ? 'r' : '-';
  permissions[8] = (mode & S_IWOTH) ? 'w' : '-';
  permissions[9] = (mode & S_IXOTH) ? 'x' : '-';

  permissions[10] = '\0';

  printf("%s", permissions);
}

static bool createFullPath(const char *directory,
                           const char *name,
                           char *fullPath,
                           size_t fullPathSize)
{
  if (directory == NULL ||
      name == NULL ||
      fullPath == NULL ||
      fullPathSize == 0)
  {
    return false;
  }

  size_t directoryLength = strlen(directory);
  size_t nameLength = strlen(name);

  bool needsSlash =
      directoryLength > 0 &&
      directory[directoryLength - 1] != '/';

  size_t requiredLength =
      directoryLength +
      (needsSlash ? 1 : 0) +
      nameLength;

  if (requiredLength >= fullPathSize)
  {
    fprintf(stderr, "ls: path is too long\n");
    return false;
  }

  strcpy(fullPath, directory);

  if (needsSlash)
  {
    strcat(fullPath, "/");
  }

  strcat(fullPath, name);

  return true;
}

static void printLongEntry(const char *displayName,
                           const char *completePath)
{
  struct stat fileInformation;

  /*
   * lstat() gives information about the symbolic link itself,
   * instead of following it.
   */
  if (lstat(completePath, &fileInformation) == -1)
  {
    perror("ls");
    return;
  }

  printPermissions(fileInformation.st_mode);

  printf(
      " %lu",
      static_cast<unsigned long>(fileInformation.st_nlink));

  struct passwd *ownerInformation =
      getpwuid(fileInformation.st_uid);

  if (ownerInformation != NULL)
  {
    printf(" %s", ownerInformation->pw_name);
  }
  else
  {
    printf(
        " %u",
        static_cast<unsigned int>(fileInformation.st_uid));
  }

  struct group *groupInformation =
      getgrgid(fileInformation.st_gid);

  if (groupInformation != NULL)
  {
    printf(" %s", groupInformation->gr_name);
  }
  else
  {
    printf(
        " %u",
        static_cast<unsigned int>(fileInformation.st_gid));
  }

  printf(
      " %lld",
      static_cast<long long>(fileInformation.st_size));

  char timeBuffer[64];

  struct tm *modificationTime =
      localtime(&fileInformation.st_mtime);

  if (modificationTime != NULL)
  {
    strftime(
        timeBuffer,
        sizeof(timeBuffer),
        "%b %d %H:%M",
        modificationTime);

    printf(" %s", timeBuffer);
  }
  else
  {
    printf(" unknown-time");
  }

  printf(" %s", displayName);

  /*
   * For a symbolic link, show its target:
   *
   * link -> target
   */
  if (S_ISLNK(fileInformation.st_mode))
  {
    char linkTarget[PATH_MAX];

    ssize_t targetLength = readlink(
        completePath,
        linkTarget,
        sizeof(linkTarget) - 1);

    if (targetLength >= 0)
    {
      linkTarget[targetLength] = '\0';
      printf(" -> %s", linkTarget);
    }
  }

  printf("\n");
}

static int compareNames(const void *first,
                        const void *second)
{
  const char *firstName =
      static_cast<const char *>(first);

  const char *secondName =
      static_cast<const char *>(second);

  return strcmp(firstName, secondName);
}

static int listDirectory(const char *directoryPath,
                         const LsOptions &options)
{
  DIR *directory = opendir(directoryPath);

  if (directory == NULL)
  {
    perror("ls");
    return -1;
  }

  /*
   * First determine how many entries must be stored.
   */
  int entryCount = 0;
  struct dirent *entry;

  while ((entry = readdir(directory)) != NULL)
  {
    if (!options.showHidden &&
        entry->d_name[0] == '.')
    {
      continue;
    }

    entryCount++;
  }

  rewinddir(directory);

  /*
   * Allocate enough space for all names.
   *
   * Each row stores one filename.
   */
  char (*names)[NAME_MAX + 1] = NULL;

  if (entryCount > 0)
  {
    names = static_cast<char (*)[NAME_MAX + 1]>(
        malloc(
            static_cast<size_t>(entryCount) *
            sizeof(*names)));

    if (names == NULL)
    {
      fprintf(stderr, "ls: memory allocation failed\n");
      closedir(directory);
      return -1;
    }
  }

  int index = 0;

  while ((entry = readdir(directory)) != NULL)
  {
    if (!options.showHidden &&
        entry->d_name[0] == '.')
    {
      continue;
    }

    strncpy(
        names[index],
        entry->d_name,
        NAME_MAX);

    names[index][NAME_MAX] = '\0';
    index++;
  }

  if (closedir(directory) == -1)
  {
    perror("ls");
    free(names);
    return -1;
  }

  /*
   * qsort() is from the C standard library and does not use STL.
   */
  qsort(
      names,
      static_cast<size_t>(entryCount),
      sizeof(*names),
      compareNames);

  for (index = 0; index < entryCount; index++)
  {
    if (options.longFormat)
    {
      char completePath[PATH_MAX];

      if (!createFullPath(
              directoryPath,
              names[index],
              completePath,
              sizeof(completePath)))
      {
        continue;
      }

      printLongEntry(names[index], completePath);
    }
    else
    {
      printf("%s\n", names[index]);
    }
  }

  free(names);

  return 0;
}

static bool expandPath(const char *argument,
                       Shell &shell,
                       char *expandedPath,
                       size_t expandedPathSize)
{
  if (argument == NULL ||
      expandedPath == NULL ||
      expandedPathSize == 0)
  {
    return false;
  }

  /*
   * Ordinary path.
   */
  if (argument[0] != '~')
  {
    if (strlen(argument) >= expandedPathSize)
    {
      fprintf(stderr, "ls: path is too long\n");
      return false;
    }

    strcpy(expandedPath, argument);
    return true;
  }

  /*
   * Only "~" and "~/..." are supported.
   */
  if (argument[1] != '\0' && argument[1] != '/')
  {
    fprintf(stderr, "ls: unsupported path: %s\n", argument);
    return false;
  }

  size_t homeLength = strlen(shell.getHomeDirectory());
  size_t remainingLength = strlen(argument + 1);

  if (homeLength + remainingLength >= expandedPathSize)
  {
    fprintf(stderr, "ls: path is too long\n");
    return false;
  }

  strcpy(expandedPath, shell.getHomeDirectory());
  strcat(expandedPath, argument + 1);

  return true;
}

static int listTarget(const char *target,
                      const LsOptions &options,
                      Shell &shell)
{
  char expandedPath[PATH_MAX];

  if (!expandPath(
          target,
          shell,
          expandedPath,
          sizeof(expandedPath)))
  {
    return -1;
  }

  struct stat targetInformation;

  if (lstat(expandedPath, &targetInformation) == -1)
  {
    perror("ls");
    return -1;
  }

  if (S_ISDIR(targetInformation.st_mode))
  {
    return listDirectory(expandedPath, options);
  }

  /*
   * The target is an ordinary file, symbolic link, or another
   * non-directory filesystem object.
   */
  if (options.longFormat)
  {
    printLongEntry(target, expandedPath);
  }
  else
  {
    printf("%s\n", target);
  }

  return 0;
}

static bool parseLsFlag(const char *argument,
                        LsOptions &options)
{
  if (argument == NULL || argument[0] != '-')
  {
    return false;
  }

  /*
   * A single "-" is treated as a filename, not a flag.
   */
  if (argument[1] == '\0')
  {
    return false;
  }

  for (int index = 1; argument[index] != '\0'; index++)
  {
    if (argument[index] == 'a')
    {
      options.showHidden = true;
    }
    else if (argument[index] == 'l')
    {
      options.longFormat = true;
    }
    else
    {
      fprintf(
          stderr,
          "ls: invalid option -- %c\n",
          argument[index]);

      return false;
    }
  }

  return true;
}

int executeLs(char *command, Shell &shell)
{
  if (command == NULL)
  {
    return -1;
  }

  char commandCopy[4096];

  if (strlen(command) >= sizeof(commandCopy))
  {
    fprintf(stderr, "ls: command is too long\n");
    return -1;
  }

  strcpy(commandCopy, command);

  LsOptions options;

  options.showHidden = false;
  options.longFormat = false;

  char *targets[MAX_LS_ARGUMENTS];
  int targetCount = 0;

  /*
   * Skip the command name "ls".
   */
  char *token = strtok(commandCopy, " \t");

  if (token == NULL)
  {
    return -1;
  }

  token = strtok(NULL, " \t");

  while (token != NULL)
  {
    if (token[0] == '-' && token[1] != '\0')
    {
      if (!parseLsFlag(token, options))
      {
        return -1;
      }
    }
    else
    {
      if (targetCount >= MAX_LS_ARGUMENTS)
      {
        fprintf(stderr, "ls: too many arguments\n");
        return -1;
      }

      targets[targetCount] = token;
      targetCount++;
    }

    token = strtok(NULL, " \t");
  }

  /*
   * ls without a path lists the current directory.
   */
  if (targetCount == 0)
  {
    return listTarget(".", options, shell);
  }

  int finalResult = 0;

  for (int index = 0; index < targetCount; index++)
  {
    /*
     * When multiple paths are supplied, print a heading for
     * each path.
     */
    if (targetCount > 1)
    {
      if (index > 0)
      {
        printf("\n");
      }

      printf("%s:\n", targets[index]);
    }

    if (listTarget(targets[index], options, shell) == -1)
    {
      finalResult = -1;
    }
  }

  return finalResult;
}