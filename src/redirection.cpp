#include "redirection.hpp"

#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>

static bool isWhitespace(char character)
{
  return character == ' ' || character == '\t';
}

static void initializeRedirectionInfo(
    RedirectionInfo &information)
{
  information.hasInput = false;
  information.hasOutput = false;
  information.appendOutput = false;

  information.inputFile[0] = '\0';
  information.outputFile[0] = '\0';
}

static void initializeSavedDescriptors(
    SavedFileDescriptors &savedDescriptors)
{
  savedDescriptors.standardInput = -1;
  savedDescriptors.standardOutput = -1;
}

static bool copyFilename(const char *filenameStart,
                         size_t filenameLength,
                         char *destination,
                         size_t destinationSize)
{
  if (filenameLength == 0)
  {
    fprintf(stderr, "Redirection filename is missing\n");
    return false;
  }

  if (filenameLength >= destinationSize)
  {
    fprintf(stderr, "Redirection filename is too long\n");
    return false;
  }

  memcpy(destination, filenameStart, filenameLength);
  destination[filenameLength] = '\0';

  return true;
}

bool parseRedirections(const char *originalCommand,
                       char *executableCommand,
                       size_t executableCommandSize,
                       RedirectionInfo &information)
{
  if (originalCommand == NULL ||
      executableCommand == NULL ||
      executableCommandSize == 0)
  {
    return false;
  }

  initializeRedirectionInfo(information);

  size_t inputIndex = 0;
  size_t outputIndex = 0;

  while (originalCommand[inputIndex] != '\0')
  {
    char currentCharacter = originalCommand[inputIndex];

    /*
     * Ordinary command character.
     */
    if (currentCharacter != '<' &&
        currentCharacter != '>')
    {
      if (outputIndex + 1 >= executableCommandSize)
      {
        fprintf(stderr, "Command is too long\n");
        return false;
      }

      executableCommand[outputIndex] = currentCharacter;
      outputIndex++;
      inputIndex++;
      continue;
    }

    bool inputRedirection = currentCharacter == '<';
    bool appendRedirection = false;

    if (inputRedirection)
    {
      if (information.hasInput)
      {
        fprintf(
            stderr,
            "Multiple input redirections are not supported\n");
        return false;
      }

      information.hasInput = true;
      inputIndex++;
    }
    else
    {
      if (information.hasOutput)
      {
        fprintf(
            stderr,
            "Multiple output redirections are not supported\n");
        return false;
      }

      information.hasOutput = true;
      inputIndex++;

      /*
       * Detect >>.
       */
      if (originalCommand[inputIndex] == '>')
      {
        appendRedirection = true;
        information.appendOutput = true;
        inputIndex++;
      }
    }

    /*
     * Skip whitespace between the operator and filename.
     */
    while (isWhitespace(originalCommand[inputIndex]))
    {
      inputIndex++;
    }

    size_t filenameStart = inputIndex;

    /*
     * The filename ends at whitespace or another
     * redirection operator.
     */
    while (originalCommand[inputIndex] != '\0' &&
           !isWhitespace(originalCommand[inputIndex]) &&
           originalCommand[inputIndex] != '<' &&
           originalCommand[inputIndex] != '>')
    {
      inputIndex++;
    }

    size_t filenameLength = inputIndex - filenameStart;

    if (inputRedirection)
    {
      if (!copyFilename(
              originalCommand + filenameStart,
              filenameLength,
              information.inputFile,
              sizeof(information.inputFile)))
      {
        return false;
      }
    }
    else
    {
      if (!copyFilename(
              originalCommand + filenameStart,
              filenameLength,
              information.outputFile,
              sizeof(information.outputFile)))
      {
        return false;
      }

      information.appendOutput = appendRedirection;
    }

    /*
     * Add one space where the redirection expression was
     * removed. This prevents neighbouring command arguments
     * from joining accidentally.
     */
    if (outputIndex > 0 &&
        !isWhitespace(executableCommand[outputIndex - 1]))
    {
      if (outputIndex + 1 >= executableCommandSize)
      {
        fprintf(stderr, "Command is too long\n");
        return false;
      }

      executableCommand[outputIndex] = ' ';
      outputIndex++;
    }
  }

  /*
   * Remove trailing spaces and tabs.
   */
  while (outputIndex > 0 &&
         isWhitespace(executableCommand[outputIndex - 1]))
  {
    outputIndex--;
  }

  executableCommand[outputIndex] = '\0';

  if (executableCommand[0] == '\0')
  {
    fprintf(stderr, "Command is missing\n");
    return false;
  }

  return true;
}

void restoreRedirections(
    SavedFileDescriptors &savedDescriptors)
{
  if (savedDescriptors.standardInput != -1)
  {
    if (dup2(
            savedDescriptors.standardInput,
            STDIN_FILENO) == -1)
    {
      perror("dup2");
    }

    if (close(savedDescriptors.standardInput) == -1)
    {
      perror("close");
    }

    savedDescriptors.standardInput = -1;
  }

  if (savedDescriptors.standardOutput != -1)
  {
    if (dup2(
            savedDescriptors.standardOutput,
            STDOUT_FILENO) == -1)
    {
      perror("dup2");
    }

    if (close(savedDescriptors.standardOutput) == -1)
    {
      perror("close");
    }

    savedDescriptors.standardOutput = -1;
  }
}

bool applyRedirections(
    const RedirectionInfo &information,
    bool saveOriginalDescriptors,
    SavedFileDescriptors &savedDescriptors)
{
  initializeSavedDescriptors(savedDescriptors);

  if (saveOriginalDescriptors && information.hasInput)
  {
    savedDescriptors.standardInput = dup(STDIN_FILENO);

    if (savedDescriptors.standardInput == -1)
    {
      perror("dup");
      return false;
    }
  }

  if (saveOriginalDescriptors && information.hasOutput)
  {
    savedDescriptors.standardOutput = dup(STDOUT_FILENO);

    if (savedDescriptors.standardOutput == -1)
    {
      perror("dup");
      restoreRedirections(savedDescriptors);
      return false;
    }
  }

  if (information.hasInput)
  {
    int inputDescriptor = open(
        information.inputFile,
        O_RDONLY);

    if (inputDescriptor == -1)
    {
      perror(information.inputFile);
      restoreRedirections(savedDescriptors);
      return false;
    }

    if (dup2(inputDescriptor, STDIN_FILENO) == -1)
    {
      perror("dup2");
      close(inputDescriptor);
      restoreRedirections(savedDescriptors);
      return false;
    }

    if (close(inputDescriptor) == -1)
    {
      perror("close");
      restoreRedirections(savedDescriptors);
      return false;
    }
  }

  if (information.hasOutput)
  {
    int outputFlags =
        O_WRONLY |
        O_CREAT;

    if (information.appendOutput)
    {
      outputFlags |= O_APPEND;
    }
    else
    {
      outputFlags |= O_TRUNC;
    }

    int outputDescriptor = open(
        information.outputFile,
        outputFlags,
        0644);

    if (outputDescriptor == -1)
    {
      perror(information.outputFile);
      restoreRedirections(savedDescriptors);
      return false;
    }

    if (dup2(outputDescriptor, STDOUT_FILENO) == -1)
    {
      perror("dup2");
      close(outputDescriptor);
      restoreRedirections(savedDescriptors);
      return false;
    }

    if (close(outputDescriptor) == -1)
    {
      perror("close");
      restoreRedirections(savedDescriptors);
      return false;
    }
  }

  return true;
}
