#include "shell.hpp"

int main()
{
  Shell shell;

  if (!shell.initialize())
  {
    return 1;
  }

  shell.run();

  return 0;
}
