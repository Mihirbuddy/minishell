# UNIX Shell

## Project Overview

This project implements a user-defined interactive UNIX shell in C++. It reads commands from the user, validates and parses them, executes built-in or external commands, and displays the prompt again after execution.

The shell supports command sequencing, foreground and background processes, pipelines, I/O redirection, signal handling, autocomplete, and persistent command history. The implementation is modular and does not use STL, `filesystem`, `system()`, `popen()`, `pclose()`, or ncurses.

The directory from which the shell is launched is treated as its home directory and is displayed as `~` in the prompt.

## Implemented Features

- Dynamic prompt containing the username, hostname, and current directory
- Semicolon-separated command execution
- Spaces and tabs between commands and arguments
- Foreground and background process execution
- Input redirection using `<`
- Output redirection using `>` and `>>`
- Pipelines containing any number of commands
- Redirection within pipelines
- `Ctrl+C`, `Ctrl+Z`, and `Ctrl+D` handling
- Command and file/directory autocomplete using `Tab`
- Persistent history of the latest 20 commands
- History navigation using the Up and Down arrow keys
- Background-child cleanup to prevent zombie processes

## Implemented Commands

The following commands are implemented directly by the shell:

- `cd`: Changes the current directory. It supports `.`, `..`, `-`, `~`, and no argument.
- `pwd`: Prints the absolute path of the current directory.
- `echo`: Prints the supplied text while handling spaces and tabs.
- `ls`: Lists files and directories. It supports `-a`, `-l`, combined flags, files, and multiple directory arguments.
- `search`: Recursively searches for a file or directory below the current directory.
- `pinfo`: Displays process status, virtual memory, and executable path on Linux.
- `history`: Displays recent commands; `history <num>` displays the requested number.
- `exit`: Terminates the custom shell.

Commands that are not implemented internally, such as `cat`, `grep`, `sort`, `sleep`, `vi`, and user-created executables, are executed as external system commands.

## Important System Calls

- `fork()`: Creates a child process for an external command or pipeline stage.
- `execvp()`: Replaces a child process with the requested external program.
- `waitpid()`: Waits for foreground processes and collects completed child processes.
- `pipe()`: Creates a communication channel between pipeline commands.
- `dup()` and `dup2()`: Save, replace, and restore standard input/output file descriptors.
- `open()` and `close()`: Open redirection files and close unused file descriptors.
- `chdir()` and `getcwd()`: Change and obtain the current working directory.
- `opendir()`, `readdir()`, and `stat()`: Read directories and obtain file information.
- `sigaction()` and `kill()`: Install signal handlers and deliver signals to foreground processes.
- `setpgid()` and `tcsetpgrp()`: Manage process groups and terminal access.

## Execution Flow

1. The shell initializes its home directory, history, terminal settings, and signal handlers.
2. It displays the prompt and reads a command line.
3. The input is stored in history and separated into commands using `;`.
4. Each command is parsed for background execution, pipelines, redirection, arguments, and flags.
5. Built-in commands are executed by the shell. External commands are executed in child processes using `fork()` and `execvp()`.
6. The parent waits for foreground processes but immediately displays the prompt for background processes.
7. The shell displays a new prompt and waits for further input.

## Compilation and Execution

Run these commands from the project directory:

```bash
make
./a.out
```

To clean and rebuild the project:

```bash
make clean
make
./a.out
```

`pinfo` must be tested on Linux because it reads process information from `/proc`.

## Sample Test Cases

### Built-in commands

```bash
echo hello world
pwd
search README.md
```

Expected output:

```text
hello world
<absolute-current-directory>
True
```

### Semicolon-separated commands

```bash
echo first ; echo second ; pwd
```

Expected output:

```text
first
second
<absolute-current-directory>
```

### Background process

```bash
sleep 2 &
```

Expected result: the PID is printed and the prompt returns immediately.

### Redirection

```bash
echo hello > output.txt
echo world >> output.txt
cat < output.txt
```

Expected output:

```text
hello
world
```

### Pipeline

```bash
echo hello | wc -c
```

Expected output:

```text
6
```

### Pipeline with redirection

```bash
echo banana > input.txt
echo apple >> input.txt
cat < input.txt | sort > sorted.txt
cat sorted.txt
```

Expected output:

```text
apple
banana
```

### History and autocomplete

```bash
history
history 3
```

`history` displays at most 10 recent commands, while `history 3` displays the latest three. The shell stores at most 20 commands across sessions.

Type part of a command or filename and press `Tab` to test autocomplete. Use the Up and Down arrow keys to navigate through history.

### Signals

Run:

```bash
sleep 100
```

- `Ctrl+C` interrupts the foreground process without terminating the shell.
- `Ctrl+Z` stops the foreground process and returns control to the shell.
- `Ctrl+D` exits the custom shell without closing the actual terminal.
