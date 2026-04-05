#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <readline/history.h>
#include <readline/readline.h>

#include "ast.h"
#include "autocmp.h"
#include "cmd.h"
#include "job.h"
#include "parse.h"
#include "ptr_array.h"
#include "scan.h"
#include "sig.h"
#include "token.h"

#define SHELL_VERSION "0.1.0"

static void write_history_file(void) {
    const char *histfile = getenv("HISTFILE");
    if (histfile != NULL) {
        write_history(histfile);
    }
}

static void setup(void) {
    rl_attempted_completion_function = shell_completion;

    using_history();
    const char *histfile = getenv("HISTFILE");
    if (histfile != NULL) {
        read_history(histfile);
    }
    atexit(write_history_file);

    signals_init();
}

static void load_config(void) {
    const char *home = getenv("HOME");
    if (home == NULL) {
        return;
    }

    char path[4096];
    snprintf(path, sizeof(path), "%s/.shellrc", home);

    FILE *f = fopen(path, "r");
    if (f == NULL) {
        return;
    }

    char line[4096];
    while (fgets(line, sizeof(line), f) != NULL) {
        // Strip trailing newline
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n') {
            line[len - 1] = '\0';
        }
        // Skip empty lines and comments
        if (line[0] == '\0' || line[0] == '#') {
            continue;
        }

        PtrArray *tokens = scan(line);
        AstNode *ast = parse(tokens);
        execute_ast(ast);
        ast_destroy(ast);
        ptr_array_destroy(tokens, token_destroy);
    }

    fclose(f);
}

static void execute_line(const char *line) {
    PtrArray *tokens = scan(line);
    AstNode *ast = parse(tokens);
    execute_ast(ast);
    ast_destroy(ast);
    ptr_array_destroy(tokens, token_destroy);
}

static void print_usage(void) {
    printf("Usage: shell [options]\n");
    printf("\n");
    printf("A feature-rich POSIX-like shell written in C.\n");
    printf("\n");
    printf("Options:\n");
    printf("  -c COMMAND    Execute COMMAND and exit\n");
    printf("  --help        Display this help and exit\n");
    printf("  --version     Display version and exit\n");
    printf("\n");
    printf("Builtins: bg, cd, echo, exit, export, fg, history, jobs, pwd, type, unset\n");
    printf("\n");
    printf("Features:\n");
    printf("  Pipelines (|), redirections (<, >, >>)\n");
    printf("  Logical operators (&&, ||), sequences (;)\n");
    printf("  Variable expansion ($VAR, ${VAR:-default}, $?, $$)\n");
    printf("  Tilde expansion (~, ~user)\n");
    printf("  Glob expansion (*, ?, [...])\n");
    printf("  Background execution (&) with job control (jobs, fg, bg)\n");
    printf("  Signal handling (Ctrl+C, Ctrl+Z)\n");
    printf("  Tab completion, command history\n");
}

int main(int argc, char *argv[]) {
    // Handle command-line flags
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_usage();
            return 0;
        }
        if (strcmp(argv[i], "--version") == 0) {
            printf("shell-c %s\n", SHELL_VERSION);
            return 0;
        }
        if (strcmp(argv[i], "-c") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "shell: -c requires an argument\n");
                return 1;
            }
            signals_init();
            execute_line(argv[i + 1]);
            return get_last_exit_status();
        }
    }

    setup();
    load_config();

    char *line;
    while ((line = readline("$ ")) != NULL) {
        // Skip empty lines
        if (*line == '\0') {
            free(line);
            continue;
        }

        add_history(line);
        execute_line(line);
        free(line);

        // Reap any finished background jobs
        job_update();
    }

    printf("\n");
    return get_last_exit_status();
}
