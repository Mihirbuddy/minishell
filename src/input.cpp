#include "input.hpp"

#include "builtins.hpp"
#include "prompt.hpp"
#include "history.hpp"

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <limits.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>

#define MAX_AUTOCOMPLETE_MATCHES 2048
#define MAX_MATCH_LENGTH 512
#define PATH_COPY_SIZE 16384
#define INPUT_HISTORY_BUFFER_SIZE 4096

struct AutocompleteMatches
{
  char values[MAX_AUTOCOMPLETE_MATCHES][MAX_MATCH_LENGTH];
  int count;
};

static bool isWhitespace(char character)
{
  return character == ' ' || character == '\t';
}

static bool isTokenSeparator(char character)
{
  return isWhitespace(character) ||
         character == ';' ||
         character == '|' ||
         character == '<' ||
         character == '>' ||
         character == '&';
}

static bool startsWith(const char *text,
                       const char *prefix)
{
  if (text == NULL || prefix == NULL)
  {
    return false;
  }

  size_t prefixLength = strlen(prefix);

  return strncmp(text, prefix, prefixLength) == 0;
}

static bool matchAlreadyExists(
    const AutocompleteMatches &matches,
    const char *value)
{
  for (int index = 0; index < matches.count; index++)
  {
    if (strcmp(matches.values[index], value) == 0)
    {
      return true;
    }
  }

  return false;
}

static void addMatch(
    AutocompleteMatches &matches,
    const char *value)
{
  if (value == NULL ||
      matches.count >= MAX_AUTOCOMPLETE_MATCHES ||
      strlen(value) >= MAX_MATCH_LENGTH ||
      matchAlreadyExists(matches, value))
  {
    return;
  }

  strcpy(matches.values[matches.count], value);
  matches.count++;
}

static int compareMatches(const void *first,
                          const void *second)
{
  const char *firstValue =
      static_cast<const char *>(first);

  const char *secondValue =
      static_cast<const char *>(second);

  return strcmp(firstValue, secondValue);
}

static void sortMatches(AutocompleteMatches &matches)
{
  qsort(
      matches.values,
      static_cast<size_t>(matches.count),
      sizeof(matches.values[0]),
      compareMatches);
}

static bool createCompletePath(
    const char *directory,
    const char *filename,
    char *completePath,
    size_t completePathSize)
{
  if (directory == NULL ||
      filename == NULL ||
      completePath == NULL)
  {
    return false;
  }

  size_t directoryLength = strlen(directory);
  size_t filenameLength = strlen(filename);

  bool slashRequired =
      directoryLength > 0 &&
      directory[directoryLength - 1] != '/';

  size_t requiredLength =
      directoryLength +
      (slashRequired ? 1 : 0) +
      filenameLength;

  if (requiredLength >= completePathSize)
  {
    return false;
  }

  strcpy(completePath, directory);

  if (slashRequired)
  {
    strcat(completePath, "/");
  }

  strcat(completePath, filename);

  return true;
}

static void collectBuiltinMatches(
    const char *prefix,
    AutocompleteMatches &matches)
{
  int builtinCount = getBuiltinCommandCount();

  for (int index = 0; index < builtinCount; index++)
  {
    const char *builtinName =
        getBuiltinCommandName(index);

    if (builtinName != NULL &&
        startsWith(builtinName, prefix))
    {
      addMatch(matches, builtinName);
    }
  }
}

static void collectPathCommandMatches(
    const char *prefix,
    AutocompleteMatches &matches)
{
  const char *pathEnvironment = getenv("PATH");

  if (pathEnvironment == NULL)
  {
    return;
  }

  char pathCopy[PATH_COPY_SIZE];

  if (strlen(pathEnvironment) >= sizeof(pathCopy))
  {
    return;
  }

  strcpy(pathCopy, pathEnvironment);

  char *directoryPath = strtok(pathCopy, ":");

  while (directoryPath != NULL)
  {
    DIR *directory = opendir(directoryPath);

    if (directory != NULL)
    {
      struct dirent *entry;

      while ((entry = readdir(directory)) != NULL)
      {
        if (!startsWith(entry->d_name, prefix))
        {
          continue;
        }

        char completePath[PATH_MAX];

        if (!createCompletePath(
                directoryPath,
                entry->d_name,
                completePath,
                sizeof(completePath)))
        {
          continue;
        }

        struct stat fileInformation;

        if (stat(completePath, &fileInformation) == -1)
        {
          continue;
        }

        /*
         * Only suggest executable non-directory files as
         * commands.
         */
        if (!S_ISDIR(fileInformation.st_mode) &&
            access(completePath, X_OK) == 0)
        {
          addMatch(matches, entry->d_name);
        }
      }

      closedir(directory);
    }

    directoryPath = strtok(NULL, ":");
  }
}

static void collectCommandMatches(
    const char *prefix,
    AutocompleteMatches &matches)
{
  collectBuiltinMatches(prefix, matches);
  collectPathCommandMatches(prefix, matches);
}

static void collectFileMatches(
    const char *prefix,
    AutocompleteMatches &matches)
{
  DIR *directory = opendir(".");

  if (directory == NULL)
  {
    return;
  }

  struct dirent *entry;

  while ((entry = readdir(directory)) != NULL)
  {
    /*
     * Hidden files are suggested only when the typed prefix
     * itself begins with '.'.
     */
    if (entry->d_name[0] == '.' &&
        prefix[0] != '.')
    {
      continue;
    }

    if (!startsWith(entry->d_name, prefix))
    {
      continue;
    }

    char value[MAX_MATCH_LENGTH];

    if (strlen(entry->d_name) >= sizeof(value))
    {
      continue;
    }

    strcpy(value, entry->d_name);

    struct stat fileInformation;

    if (stat(entry->d_name, &fileInformation) == 0 &&
        S_ISDIR(fileInformation.st_mode))
    {
      /*
       * Add '/' after directories so the user can continue
       * entering a path.
       */
      if (strlen(value) + 1 < sizeof(value))
      {
        strcat(value, "/");
      }
    }

    addMatch(matches, value);
  }

  closedir(directory);
}

static size_t findCurrentTokenStart(
    const char *input,
    size_t inputLength)
{
  size_t tokenStart = inputLength;

  while (tokenStart > 0 &&
         !isTokenSeparator(input[tokenStart - 1]))
  {
    tokenStart--;
  }

  return tokenStart;
}

static bool currentTokenIsCommand(
    const char *input,
    size_t tokenStart)
{
  if (input == NULL)
  {
    return false;
  }

  /*
   * Locate the start of the current semicolon or pipeline
   * segment.
   */
  size_t segmentStart = tokenStart;

  while (segmentStart > 0)
  {
    char previous = input[segmentStart - 1];

    if (previous == ';' || previous == '|')
    {
      break;
    }

    segmentStart--;
  }

  while (segmentStart < tokenStart &&
         isWhitespace(input[segmentStart]))
  {
    segmentStart++;
  }

  /*
   * The first token after ';' or '|' is a command.
   */
  return segmentStart == tokenStart;
}

static size_t findCommonPrefixLength(
    const AutocompleteMatches &matches)
{
  if (matches.count <= 0)
  {
    return 0;
  }

  size_t commonLength = strlen(matches.values[0]);

  for (int matchIndex = 1;
       matchIndex < matches.count;
       matchIndex++)
  {
    size_t currentLength = 0;

    while (currentLength < commonLength &&
           matches.values[0][currentLength] ==
               matches.values[matchIndex][currentLength])
    {
      currentLength++;
    }

    commonLength = currentLength;
  }

  return commonLength;
}

static bool appendText(
    char *input,
    size_t &inputLength,
    size_t inputSize,
    const char *text)
{
  if (text == NULL)
  {
    return false;
  }

  size_t textLength = strlen(text);

  if (inputLength + textLength >= inputSize)
  {
    return false;
  }

  memcpy(input + inputLength, text, textLength);

  inputLength += textLength;
  input[inputLength] = '\0';

  if (write(STDOUT_FILENO, text, textLength) == -1)
  {
    return false;
  }

  return true;
}

static void displayMatches(
    const AutocompleteMatches &matches,
    const char *homeDirectory,
    const char *input)
{
  const char newline = '\n';
  write(STDOUT_FILENO, &newline, 1);

  for (int index = 0; index < matches.count; index++)
  {
    write(
        STDOUT_FILENO,
        matches.values[index],
        strlen(matches.values[index]));

    if (index == matches.count - 1)
    {
      write(STDOUT_FILENO, "\n", 1);
    }
    else
    {
      write(STDOUT_FILENO, "  ", 2);
    }
  }

  /*
   * Reprint the prompt and the command currently being entered.
   */
  displayPrompt(homeDirectory);

  write(
      STDOUT_FILENO,
      input,
      strlen(input));
}

static void performAutocomplete(
    char *input,
    size_t &inputLength,
    size_t inputSize,
    const char *homeDirectory,
    bool repeatedTab)
{
  size_t tokenStart = findCurrentTokenStart(
      input,
      inputLength);

  char prefix[MAX_MATCH_LENGTH];

  size_t prefixLength = inputLength - tokenStart;

  if (prefixLength >= sizeof(prefix))
  {
    return;
  }

  memcpy(prefix, input + tokenStart, prefixLength);
  prefix[prefixLength] = '\0';

  AutocompleteMatches matches;
  matches.count = 0;

  bool commandToken = currentTokenIsCommand(
      input,
      tokenStart);

  if (commandToken)
  {
    collectCommandMatches(prefix, matches);
  }
  else
  {
    collectFileMatches(prefix, matches);
  }

  if (matches.count == 0)
  {
    /*
     * Audible terminal bell for no match.
     */
    write(STDOUT_FILENO, "\a", 1);
    return;
  }

  sortMatches(matches);

  if (matches.count == 1)
  {
    const char *match = matches.values[0];

    const char *remainingText =
        match + prefixLength;

    appendText(
        input,
        inputLength,
        inputSize,
        remainingText);

    /*
     * Add a space after a complete command or ordinary file.
     * Do not add a space after a directory ending in '/'.
     */
    size_t matchLength = strlen(match);

    if (matchLength > 0 &&
        match[matchLength - 1] != '/')
    {
      appendText(
          input,
          inputLength,
          inputSize,
          " ");
    }

    return;
  }

  size_t commonPrefixLength =
      findCommonPrefixLength(matches);

  /*
   * First Tab fills the common prefix, if it is longer than the
   * prefix already typed.
   */
  if (commonPrefixLength > prefixLength &&
      !repeatedTab)
  {
    char remainingText[MAX_MATCH_LENGTH];

    size_t remainingLength =
        commonPrefixLength - prefixLength;

    memcpy(
        remainingText,
        matches.values[0] + prefixLength,
        remainingLength);

    remainingText[remainingLength] = '\0';

    appendText(
        input,
        inputLength,
        inputSize,
        remainingText);

    return;
  }

  /*
   * If no additional common prefix exists, or Tab was pressed
   * repeatedly, display every match.
   */
  displayMatches(
      matches,
      homeDirectory,
      input);
}

static bool enableRawInputMode(
    struct termios &originalTerminal)
{
  if (tcgetattr(STDIN_FILENO, &originalTerminal) == -1)
  {
    perror("tcgetattr");
    return false;
  }

  struct termios rawTerminal = originalTerminal;

  /*
   * Disable canonical input and automatic echo.
   *
   * Keep ISIG enabled so Ctrl+C and Ctrl+Z still generate signals.
   */
  rawTerminal.c_lflag &= ~(ICANON | ECHO);

  rawTerminal.c_cc[VMIN] = 1;
  rawTerminal.c_cc[VTIME] = 0;

  if (tcsetattr(
          STDIN_FILENO,
          TCSAFLUSH,
          &rawTerminal) == -1)
  {
    perror("tcsetattr");
    return false;
  }

  return true;
}

static void restoreTerminalMode(
    const struct termios &originalTerminal)
{
  if (tcsetattr(
          STDIN_FILENO,
          TCSAFLUSH,
          &originalTerminal) == -1)
  {
    perror("tcsetattr");
  }
}

static void replaceDisplayedInput(
    char *input,
    size_t &inputLength,
    size_t inputSize,
    const char *replacement,
    const char *homeDirectory)
{
  if (replacement == NULL)
  {
    replacement = "";
  }

  size_t replacementLength = strlen(replacement);

  if (replacementLength >= inputSize)
  {
    return;
  }

  strcpy(input, replacement);
  inputLength = replacementLength;

  /*
   * Move to the beginning of the line and clear it.
   */
  write(STDOUT_FILENO, "\r\033[2K", 5);

  displayPrompt(homeDirectory);

  write(
      STDOUT_FILENO,
      input,
      inputLength);
}

int readInputLine(
    char *input,
    size_t inputSize,
    const char *homeDirectory)
{
  if (input == NULL || inputSize == 0)
  {
    return INPUT_ERROR;
  }

  struct termios originalTerminal;

  if (!enableRawInputMode(originalTerminal))
  {
    return INPUT_ERROR;
  }

  size_t inputLength = 0;
  bool previousCharacterWasTab = false;

  int historyIndex = getHistoryCount();

  char originalInput[INPUT_HISTORY_BUFFER_SIZE];
  originalInput[0] = '\0';

  bool historyNavigationStarted = false;

  input[0] = '\0';

  while (true)
  {
    char character;

    ssize_t bytesRead = read(
        STDIN_FILENO,
        &character,
        1);

    if (bytesRead == -1)
    {
      int savedError = errno;

      restoreTerminalMode(originalTerminal);

      if (savedError == EINTR)
      {
        return INPUT_INTERRUPTED;
      }

      errno = savedError;
      perror("read");
      return INPUT_ERROR;
    }

    if (bytesRead == 0)
    {
      restoreTerminalMode(originalTerminal);
      return INPUT_EOF;
    }

    /*
     * Enter.
     */
    if (character == '\n' || character == '\r')
    {
      write(STDOUT_FILENO, "\n", 1);

      input[inputLength] = '\0';

      restoreTerminalMode(originalTerminal);
      return INPUT_SUCCESS;
    }

    /*
     * Ctrl+D.
     */
    if (character == 4)
    {
      if (inputLength == 0)
      {
        write(STDOUT_FILENO, "\n", 1);

        restoreTerminalMode(originalTerminal);
        return INPUT_EOF;
      }

      previousCharacterWasTab = false;
      continue;
    }

    /*
     * Tab.
     */
    if (character == '\t')
    {
      performAutocomplete(
          input,
          inputLength,
          inputSize,
          homeDirectory,
          previousCharacterWasTab);

      previousCharacterWasTab = true;
      continue;
    }

    /*
     * Backspace: support both DEL and Ctrl+H.
     */
    if (character == 127 || character == 8)
    {
      if (inputLength > 0)
      {
        inputLength--;
        input[inputLength] = '\0';

        write(STDOUT_FILENO, "\b \b", 3);
      }

      previousCharacterWasTab = false;
      continue;
    }

    /*
     * Ignore escape sequences for now.
     *
     * Arrow-key history navigation will be implemented in a
     * later phase.
     */
    if (character == 27)
    {
      char openingBracket;
      char arrowCode;

      ssize_t firstRead = read(
          STDIN_FILENO,
          &openingBracket,
          1);

      ssize_t secondRead = read(
          STDIN_FILENO,
          &arrowCode,
          1);

      if (firstRead != 1 ||
          secondRead != 1 ||
          openingBracket != '[')
      {
        previousCharacterWasTab = false;
        continue;
      }

      int currentHistoryCount = getHistoryCount();

      /*
       * Up arrow: ESC [ A
       */
      if (arrowCode == 'A')
      {
        if (currentHistoryCount == 0)
        {
          continue;
        }

        /*
         * Save the text currently being entered before beginning
         * history navigation.
         */
        if (!historyNavigationStarted)
        {
          strncpy(
              originalInput,
              input,
              sizeof(originalInput) - 1);

          originalInput[sizeof(originalInput) - 1] = '\0';

          historyNavigationStarted = true;
          historyIndex = currentHistoryCount;
        }

        if (historyIndex > 0)
        {
          historyIndex--;
        }

        const char *historyCommand =
            getHistoryCommand(historyIndex);

        replaceDisplayedInput(
            input,
            inputLength,
            inputSize,
            historyCommand,
            homeDirectory);
      }

      /*
       * Down arrow: ESC [ B
       */
      else if (arrowCode == 'B')
      {
        /*
         * Down arrow works only after Up has been pressed.
         */
        if (!historyNavigationStarted)
        {
          continue;
        }

        if (historyIndex < currentHistoryCount - 1)
        {
          historyIndex++;

          replaceDisplayedInput(
              input,
              inputLength,
              inputSize,
              getHistoryCommand(historyIndex),
              homeDirectory);
        }
        else if (historyIndex ==
                 currentHistoryCount - 1)
        {
          /*
           * Move below the newest history command and restore
           * whatever was entered before Up was first pressed.
           */
          historyIndex = currentHistoryCount;

          replaceDisplayedInput(
              input,
              inputLength,
              inputSize,
              originalInput,
              homeDirectory);
        }

        /*
         * If already below the latest command, remain there.
         */
      }

      previousCharacterWasTab = false;
      continue;
    }

    /*
     * Ignore non-printable control characters.
     */
    if (character < 32 || character > 126)
    {
      previousCharacterWasTab = false;
      continue;
    }

    if (inputLength + 1 >= inputSize)
    {
      write(STDOUT_FILENO, "\a", 1);
      previousCharacterWasTab = false;
      continue;
    }

    input[inputLength] = character;
    inputLength++;

    input[inputLength] = '\0';

    write(STDOUT_FILENO, &character, 1);

    previousCharacterWasTab = false;
  }
}