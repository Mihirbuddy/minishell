#ifndef PIPELINE_HPP
#define PIPELINE_HPP

class Shell;

int executePipeline(
    char *command,
    bool background,
    Shell &shell);

#endif