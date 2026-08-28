#include "prompt.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits.h>
#include <pwd.h>
#include <sys/types.h>
#include <unistd.h>

#define HOSTNAME_BUFFER_SIZE 256

static const char* getUsername() {
    uid_t userId = getuid();

    struct passwd* userInformation = getpwuid(userId);

    if (userInformation != NULL &&
        userInformation->pw_name != NULL) {
        return userInformation->pw_name;
    }

    /*
     * getenv("USER") is only used as a fallback.
     * The username is not hard-coded.
     */
    const char* environmentUsername = getenv("USER");

    if (environmentUsername != NULL) {
        return environmentUsername;
    }

    return "unknown";
}

static void printDisplayPath(const char* currentDirectory,
                             const char* homeDirectory) {
    size_t homeLength = strlen(homeDirectory);

    /*
     * Case 1:
     * Current directory is exactly the shell home directory.
     */
    if (strcmp(currentDirectory, homeDirectory) == 0) {
        printf("~");
        return;
    }

    /*
     * Case 2:
     * Current directory is inside the shell home directory.
     *
     * The additional '/' check prevents a path such as:
     *
     * homeDirectory = /home/mihir/test
     * currentPath   = /home/mihir/testing
     *
     * from incorrectly becoming ~/ing.
     */
    if (strncmp(currentDirectory, homeDirectory, homeLength) == 0 &&
        currentDirectory[homeLength] == '/') {
        printf("~%s", currentDirectory + homeLength);
        return;
    }

    /*
     * Case 3:
     * Current directory is outside the shell home directory.
     */
    printf("%s", currentDirectory);
}

void displayPrompt(const char* homeDirectory) {
    char hostname[HOSTNAME_BUFFER_SIZE];
    char currentDirectory[PATH_MAX];

    if (gethostname(hostname, sizeof(hostname)) == -1) {
        perror("gethostname");
        strcpy(hostname, "unknown");
    }

    /*
     * gethostname() may not add '\0' if the hostname is too long.
     */
    hostname[sizeof(hostname) - 1] = '\0';

    if (getcwd(currentDirectory, sizeof(currentDirectory)) == NULL) {
        perror("getcwd");
        strcpy(currentDirectory, "unknown");
    }

    printf("<%s@%s:", getUsername(), hostname);

    printDisplayPath(currentDirectory, homeDirectory);

    printf("> ");
    fflush(stdout);
}