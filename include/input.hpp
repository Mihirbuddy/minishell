#ifndef INPUT_HPP
#define INPUT_HPP

#include <cstddef>

enum InputResult
{
  INPUT_ERROR = -1,
  INPUT_INTERRUPTED = -2,
  INPUT_EOF = 0,
  INPUT_SUCCESS = 1
};

int readInputLine(
    char *input,
    size_t inputSize,
    const char *homeDirectory);

#endif