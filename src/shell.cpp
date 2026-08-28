#include "shell.hpp"
#include "parser.hpp"
#include "prompt.hpp"
#include "executor.hpp"

#include <cstdio>
#include <cstring>
#include <unistd.h>

#define INPUT_BUFFER_SIZE 4096

Shell::Shell()
{
  homeDirectory[0] = '\0';
  previousDirectory[0] = '\0';

  shellPid = -1;
  running = false;
}

bool Shell::initialize()
{
  /*syntax:getcwd(destinationBuffer, bufferSize);
Find the program’s current directory and place its path inside homeDirectory.
Suppose you start your shell from: /home/mihir/Assignment2
After getcwd() succeeds:
homeDirectory contains: "/home/mihir/Assignment2"

What does getcwd() return?
When successful, it returns the address of the destination buffer.
*/
  if (getcwd(homeDirectory, sizeof(homeDirectory)) == NULL)
  {
    perror("getcwd");
    return false;
  }

  if (strncpy(previousDirectory,
              homeDirectory,
              sizeof(previousDirectory) - 1) == NULL)
  {
    fprintf(stderr, "Failed to initialize previous directory\n");
    return false;
  }

  previousDirectory[sizeof(previousDirectory) - 1] = '\0';

  shellPid = getpid();
  running = true;

  return true;
}

void Shell::run()
{
  char input[INPUT_BUFFER_SIZE];

  while (running)
  {
    reapBackgroundProcesses();
    displayPrompt(homeDirectory);

    if (fgets(input, sizeof(input), stdin) == NULL)
    {
      /*
       * fgets() returning NULL can mean:
       *
       * 1. EOF was received, such as Ctrl+D.
       * 2. An input error occurred.
       */

      if (feof(stdin))
      {
        printf("\n");
        stop();
        break;
      }

      perror("fgets");
      clearerr(stdin);
      continue;
    }

    /*
     * If the input completely filled the buffer and did not contain
     * a newline, discard the remaining characters from that line.
     */
    if (strchr(input, '\n') == NULL)
    {
      int character;

      while ((character = getchar()) != '\n' &&
             character != EOF)
      {
        // Discard the remaining input.
      }

      fprintf(stderr, "Input is too long\n");
      continue;
    }

    /*
     * Remove the newline placed in the buffer by fgets().
     */
    input[strcspn(input, "\n")] = '\0';

    parseInput(input, *this);
  }
}

void Shell::stop()
{
  running = false;
}

bool Shell::isRunning() const
{
  return running;
}

const char *Shell::getHomeDirectory() const
{
  return homeDirectory;
}

const char *Shell::getPreviousDirectory() const
{
  return previousDirectory;
}

bool Shell::setPreviousDirectory(const char *path)
{
  if (path == NULL)
  {
    return false;
  }

  if (strlen(path) >= sizeof(previousDirectory))
  {
    fprintf(stderr, "Directory path is too long\n");
    return false;
  }

  strcpy(previousDirectory, path);

  return true;
}