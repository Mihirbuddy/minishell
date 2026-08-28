# MihirShell – Custom UNIX Shell

## Overview

MihirShell is a modular UNIX-style command-line shell developed in C++ as part of the Advanced Operating Systems assignment.

The shell currently supports built-in and external commands, foreground/background execution, I/O redirection, multi-stage pipelines, semicolon-separated commands, and basic signal handling.

The implementation does not use STL containers, the C++ `filesystem` library, `system()`, `popen()`, `pclose()`, or ncurses.

## Implemented Phases

### Phase 1: Shell Foundation

Implemented the basic shell structure and interactive input loop.

Features:

* Modular header and source-file structure
* Dynamic `<username@hostname:path>` prompt
* Shell home-directory tracking using `~`
* Input handling using fixed-size character arrays
* Leading and trailing whitespace removal
* Semicolon-separated command parsing
* `exit` command
* `Ctrl+D` handling
* Overly long input detection
* Makefile-based compilation

Example:

```bash
echo hello ; pwd ; exit
```

### Phase 2: Basic Built-in Commands

Implemented commands that execute directly inside the shell process.

Commands:

* `cd`
* `pwd`
* `echo`
* `exit`

Supported `cd` forms:

```bash
cd
cd .
cd ..
cd -
cd ~
cd ~/directory
```

The shell maintains its starting directory as its custom home and stores the previous directory without using the `PWD` or `OLDPWD` environment variables.

### Phase 3: External Command Execution

Added execution of commands not implemented as shell built-ins.

Features:

* Child-process creation using `fork()`
* External program execution using `execvp()`
* Foreground execution using `waitpid()`
* Background execution using `&`
* Basic cleanup of completed background processes
* Error handling for unavailable commands
* Support for launching another instance of the shell

Examples:

```bash
date
uname
sleep 5
sleep 5 &
./a.out
```

### Phase 4: Custom `ls` Command

Implemented `ls` internally without executing the system’s `/bin/ls`.

Supported forms:

```bash
ls
ls -a
ls -l
ls -la
ls -al
ls <path>
ls -l <path>
ls <path1> <path2>
```

Implementation uses:

* `opendir()`
* `readdir()`
* `closedir()`
* `lstat()`
* `readlink()`
* `getpwuid()`
* `getgrgid()`

The long format displays file type, permissions, link count, owner, group, size, modification time, filename, and symbolic-link target.

### Phase 5: I/O Redirection

Implemented input, output, and append redirection.

Supported operators:

```text
<    Input redirection
>    Output redirection with overwrite
>>   Output redirection with append
```

Examples:

```bash
cat < input.txt
echo hello > output.txt
echo world >> output.txt
sort < input.txt > sorted.txt
ls -l > files.txt
```

Redirection works with both built-in and external commands.

The implementation uses:

* `open()`
* `dup()`
* `dup2()`
* `close()`

For parent-executed built-ins, the shell saves and restores its original standard input and output descriptors after execution.

### Phase 6: Multi-stage Pipelines

Implemented pipelines containing two or more commands.

Examples:

```bash
echo hello | wc -c
cat input.txt | grep hello
cat input.txt | sort | uniq
cat < input.txt | sort | uniq > output.txt
```

Features:

* Multiple commands connected through pipes
* Concurrent execution of pipeline stages
* Built-in commands inside pipelines
* External commands inside pipelines
* Redirection combined with pipelines
* Optional background pipeline execution
* Validation of missing pipeline commands

Each pipeline stage runs in a separate child process. All unused pipe descriptors are closed to ensure that reading processes receive EOF correctly.

### Phase 7: Simple Signal Handling

Implemented the required terminal signals.

#### `Ctrl+C`

* Sends `SIGINT` to the currently running foreground command
* Interrupts the complete foreground pipeline
* Does not terminate the custom shell
* Has no effect on the shell when no foreground process is running

#### `Ctrl+Z`

* Sends `SIGTSTP` to the currently running foreground command
* Stops the complete foreground pipeline
* Returns control to the custom shell
* Has no effect on the shell when no foreground process is running

#### `Ctrl+D`

* Logs out of the custom shell using EOF handling
* Does not close or affect the actual terminal

Foreground commands and pipeline stages are assigned process groups so signals can be delivered to the complete foreground job.

## Current Built-in Commands

```text
cd
pwd
echo
ls
exit
```

Additional assignment-specific commands will be added in later phases.

## Current Project Structure

```text
2026201019_assignment2/
├── README.md
├── makefile
├── include/
│   ├── builtins.hpp
│   ├── executor.hpp
│   ├── ls.hpp
│   ├── parser.hpp
│   ├── pipeline.hpp
│   ├── prompt.hpp
│   ├── redirection.hpp
│   ├── shell.hpp
│   └── signals.hpp
└── src/
    ├── builtins.cpp
    ├── executor.cpp
    ├── ls.cpp
    ├── main.cpp
    ├── parser.cpp
    ├── pipeline.cpp
    ├── prompt.cpp
    ├── redirection.cpp
    ├── shell.cpp
    └── signals.cpp
```

## Compilation

From the project directory:

```bash
make
```

## Execution

```bash
./a.out
```

## Clean Build

```bash
make clean
make
./a.out
```

## Example Commands

```bash
pwd
cd src
echo hello world
ls -la
date
sleep 10 &
echo hello > output.txt
cat < output.txt
cat file.txt | grep hello | wc -l
cat < input.txt | sort | uniq > output.txt
pwd ; ls ; echo completed
```

## Error Handling

The shell currently handles:

* Invalid command names
* Unsupported built-in arguments
* Invalid `ls` flags
* Missing files and directories
* Excessively long input and commands
* Invalid background operator placement
* Missing redirection filenames
* Multiple conflicting redirections
* Invalid or empty pipeline stages
* Failures from `fork()`, `execvp()`, `pipe()`, `open()`, `dup()`, `dup2()`, `waitpid()`, and directory-related system calls

## Constraints Followed

* Implemented only in C++
* No STL containers
* No C++ `filesystem` library
* No `system()`
* No `popen()` or `pclose()`
* No curses or ncurses
* No use of `PWD` or `OLDPWD`
* External programs execute through the `exec` family
* Pipes use the `pipe()` system call
* Errors are handled using return values, `perror()`, and appropriate messages

## Platform Note

Most functionality can be developed on macOS, but Linux-specific commands that use files such as `/proc/<pid>/stat`, `/proc/<pid>/status`, or `/proc/<pid>/exe` must be implemented and tested on Linux.

Recommended final testing environments include Ubuntu, the IIIT Hyderabad lab system, or another Linux environment.
