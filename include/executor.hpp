#ifndef EXECUTOR_HPP
#define EXECUTOR_HPP

struct RedirectionInfo;

int executeExternalCommand(
    char *command,
    bool background,
    const RedirectionInfo &redirection);

void reapBackgroundProcesses();

#endif