# shell-c

[![CI](https://github.com/louisphilipmarcoux/shell-c/actions/workflows/ci.yml/badge.svg)](https://github.com/louisphilipmarcoux/shell-c/actions/workflows/ci.yml)
[![C23](https://img.shields.io/badge/standard-C23-blue.svg)](https://en.cppreference.com/w/c/23)
[![License: MIT](https://img.shields.io/badge/license-MIT-green.svg)](LICENSE)
[![Tests](https://img.shields.io/badge/tests-50%20passing-brightgreen.svg)](tests/run_tests.sh)

A feature-rich POSIX-like shell written in C23, featuring variable expansion, job control, and more.

## Features

- **Interactive REPL** with GNU readline (line editing, tab completion, history)
- **Pipelines** (`cmd1 | cmd2 | cmd3`)
- **I/O redirections** (`<`, `>`, `>>`, `2>`, `2>>`)
- **Logical operators** (`&&`, `||`) and command sequences (`;`)
- **Variable expansion** (`$VAR`, `${VAR}`, `${VAR:-default}`, `$?`, `$$`)
- **Tilde expansion** (`~`, `~/path`, `~user`)
- **Glob expansion** (`*`, `?`, `[...]`)
- **Background execution** (`&`) with job control (`jobs`, `fg`, `bg`)
- **Signal handling** (Ctrl+C interrupts foreground process, not the shell)
- **11 builtins**: `bg`, `cd`, `echo`, `exit`, `export`, `fg`, `history`, `jobs`, `pwd`, `type`, `unset`
- **Config file** (`~/.shellrc`) executed on startup
- **Non-interactive mode** (`shell -c "command"`)

## Architecture

```text
Input
  |
  v
scan.c          Lexical analysis: input string -> tokens
  |             Handles quotes (single, double), escapes, operators
  v
parse.c         Syntax analysis: tokens -> AST
  |             Grammar: sequences > lists > pipelines > commands
  v
ast.c           Abstract syntax tree nodes
  |             NODE_PIPELINE, NODE_AND, NODE_OR, NODE_SEQUENCE
  v
cmd.c           Execution engine
  |             fork/exec, pipes, redirections, builtins
  |
  +-- expand.c  Runtime word expansion ($VAR, ~, globs)
  +-- redir.c   File descriptor management (save/restore/redirect)
  +-- job.c     Background job tracking and control
  +-- sig.c     Signal handler setup (SIGINT, SIGTSTP, etc.)
  +-- misc.c    PATH lookup, builtin registry, utilities
```

**Supporting modules:**
- `ptr_array.c` — dynamic pointer array used throughout
- `trie.c` — prefix tree for fast tab completion
- `xmalloc.c` — allocation wrappers that exit on failure

## Building

Requires CMake 3.13+, a C23-capable compiler (GCC 13+ or Clang 16+), and libreadline-dev.

```sh
# Install dependencies (Debian/Ubuntu)
sudo apt-get install libreadline-dev cmake build-essential

# Build
cmake -B build -S .
cmake --build build

# Run
./build/shell
```

### Build with sanitizers

```sh
cmake -B build -DENABLE_SANITIZERS=ON
cmake --build build
```

## Usage

```sh
# Interactive mode
./build/shell

# Execute a command and exit
./build/shell -c "echo hello && ls | wc -l"

# Show help
./build/shell --help
```

## Testing

```sh
bash tests/run_tests.sh ./build/shell
```

50 integration tests covering builtins, pipes, redirections, variable expansion, logical operators, quoting, tilde expansion, and CLI flags.

## License

MIT
