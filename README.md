# MiniShell - Command Line Interpreter

This project consists of the implementation of a command line interpreter (MiniShell) for the **Operating Systems** course in the Software Engineering Degree (Academic Year 2025-26).

The main objective is to create a C program that acts as an interface between the user and the operating system, allowing program execution, process management, and file manipulation through redirections and pipes.

## 📋 Implemented Features

The program meets the following objectives defined in the assignment requirements:

### Command Execution
**Foreground Execution:** Sequential execution of one or several commands separated by pipes.
**Background Execution:** Non-blocking execution if the command line ends with `&`, displaying the PID of the process.
**Argument Support:** Handles multiple arguments for each command.

### Redirections and Pipes
**Input Redirection (`<`):** Reads input from a file (valid only for the first command in the pipe).
**Output Redirection (`>`):** Writes standard output to a file (valid only for the last command in the pipe).
**Error Redirection (`>&`):** Writes error output to a file (valid only for the last command in the pipe).
**Multiple Pipes:** Supports complex sequences (e.g., `cmd1 | cmd2 | cmd3`).

### Internal (Built-in) Commands
The shell includes its own implementation of the following commands:
- `cd`: Change directory.Supports absolute and relative paths, as well as the `$HOME` variable.
- `jobs`: Lists jobs currently running in the background or stopped.
- `bg`: Resumes a stopped job in the background.
- `umask`: Displays or modifies the file creation mask (octal format).
- `exit`: Terminates the shell execution in an orderly manner.

### Signal Handling
- **SIGINT (Ctrl+C):** The shell and background processes ignore it; it only affects foreground processes.
- **SIGTSTP (Ctrl+Z):** The shell does not stop. It captures the signal to stop foreground processes and manage them via `jobs`/`bg`.

## 🛠️ Requirements and Compilation

To compile the project, the `libparser.a` library and the `parser.h` header file provided by the course are required.

### File Structure
* `myshell.c`: Main source code file.
* `parser.h`: Lexical analysis header file.
* `libparser.a`: Static analysis library.

### Compilation
The project is compiled using `gcc` with warning flags enabled and static linking for the parser library:

```bash
gcc -Wall -Wextra myshell.c libparser.a -o myshell -static
