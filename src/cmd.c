#include "cmd.h"
#include "ast.h"
#include "expand.h"
#include "job.h"
#include "misc.h"
#include "ptr_array.h"
#include "redir.h"
#include "sig.h"
#include "xmalloc.h"

#include <err.h>
#include <errno.h>
#include <readline/history.h>
#include <readline/readline.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

static int last_exit_status = 0;

int get_last_exit_status(void) {
    return last_exit_status;
}

// --- Builtin commands ---

static void cmd_cd(const PtrArray *arguments) {
    const char *dir;
    if (ptr_array_get_size(arguments) < 2) {
        dir = getenv("HOME");
        if (dir == NULL) {
            fprintf(stderr, "cd: HOME not set\n");
            last_exit_status = 1;
            return;
        }
    } else {
        dir = ptr_array_get_const(arguments, 1);
    }
    if (chdir(dir) < 0) {
        fprintf(stderr, "cd: %s: %s\n", dir, strerror(errno));
        last_exit_status = 1;
        return;
    }
    last_exit_status = 0;
}

static void cmd_echo(const PtrArray *arguments) {
    size_t num_args = ptr_array_get_size(arguments);
    for (size_t i = 1; i < num_args; i++) {
        if (i > 1) {
            printf(" ");
        }
        printf("%s", (const char *)ptr_array_get_const(arguments, i));
    }
    printf("\n");
    last_exit_status = 0;
}

static void cmd_exit(const PtrArray *arguments) {
    int status = 0;
    if (ptr_array_get_size(arguments) > 1) {
        status = parse_int(ptr_array_get_const(arguments, 1), 0);
    }
    exit(status);
}

static void cmd_export(const PtrArray *arguments) {
    size_t num_args = ptr_array_get_size(arguments);
    if (num_args < 2) {
        // Print all exported variables
        extern char **environ;
        for (char **env = environ; *env != NULL; env++) {
            printf("declare -x %s\n", *env);
        }
        last_exit_status = 0;
        return;
    }

    for (size_t i = 1; i < num_args; i++) {
        const char *arg = ptr_array_get_const(arguments, i);
        const char *eq = strchr(arg, '=');
        if (eq != NULL) {
            char *name = xstrndup(arg, eq - arg);
            if (setenv(name, eq + 1, 1) < 0) {
                warn("setenv");
                last_exit_status = 1;
                free(name);
                return;
            }
            free(name);
        }
        // export without = just marks for export (no-op in this shell)
    }
    last_exit_status = 0;
}

static void cmd_unset(const PtrArray *arguments) {
    size_t num_args = ptr_array_get_size(arguments);
    for (size_t i = 1; i < num_args; i++) {
        unsetenv(ptr_array_get_const(arguments, i));
    }
    last_exit_status = 0;
}

static void cmd_history(const PtrArray *arguments) {
    static int last_append_n = 0;
    int n = history_length + 1 - history_base;

    if (ptr_array_get_size(arguments) > 2) {
        const char *option = ptr_array_get_const(arguments, 1);
        const char *histfile = ptr_array_get_const(arguments, 2);

        if (strcmp(option, "-r") == 0) {
            read_history(histfile);
        } else if (strcmp(option, "-w") == 0) {
            write_history(histfile);
        } else if (strcmp(option, "-a") == 0) {
            append_history(n - last_append_n, histfile);
            last_append_n = n;
        }
        last_exit_status = 0;
        return;
    }

    if (ptr_array_get_size(arguments) > 1) {
        n = parse_int(ptr_array_get_const(arguments, 1), n);
    }

    for (int i = history_length + 1 - n; i <= history_length; i++) {
        HIST_ENTRY *entry = history_get(i);
        if (entry != NULL) {
            printf("%5d  %s\n", i, entry->line);
        }
    }
    last_exit_status = 0;
}

static void cmd_pwd(const PtrArray *arguments) {
    char *cwd = getcwd(NULL, 0);
    if (cwd == NULL) {
        warn("getcwd");
        last_exit_status = 1;
        return;
    }
    printf("%s\n", cwd);
    free(cwd);
    last_exit_status = 0;
}

static void cmd_type(const PtrArray *arguments) {
    size_t num_args = ptr_array_get_size(arguments);
    for (size_t i = 1; i < num_args; i++) {
        const char *name = ptr_array_get_const(arguments, i);

        if (is_builtin(name)) {
            printf("%s is a shell builtin\n", name);
            continue;
        }

        char *path = find_executable(name);
        if (path != NULL) {
            printf("%s is %s\n", name, path);
            free(path);
            continue;
        }

        printf("%s: not found\n", name);
    }
    last_exit_status = 0;
}

static void cmd_jobs(const PtrArray *arguments) {
    job_list();
    last_exit_status = 0;
}

static void cmd_fg(const PtrArray *arguments) {
    int n = 1;
    if (ptr_array_get_size(arguments) > 1) {
        const char *arg = ptr_array_get_const(arguments, 1);
        if (arg[0] == '%') arg++;
        n = parse_int(arg, 1);
    }
    last_exit_status = job_fg(n);
}

static void cmd_bg(const PtrArray *arguments) {
    int n = 1;
    if (ptr_array_get_size(arguments) > 1) {
        const char *arg = ptr_array_get_const(arguments, 1);
        if (arg[0] == '%') arg++;
        n = parse_int(arg, 1);
    }
    last_exit_status = job_bg(n);
}

static void execute_builtin(const PtrArray *arguments) {
    const char *cmd_name = ptr_array_get_const(arguments, 0);
    if (strcmp(cmd_name, "bg") == 0) {
        cmd_bg(arguments);
    } else if (strcmp(cmd_name, "cd") == 0) {
        cmd_cd(arguments);
    } else if (strcmp(cmd_name, "echo") == 0) {
        cmd_echo(arguments);
    } else if (strcmp(cmd_name, "exit") == 0) {
        cmd_exit(arguments);
    } else if (strcmp(cmd_name, "export") == 0) {
        cmd_export(arguments);
    } else if (strcmp(cmd_name, "fg") == 0) {
        cmd_fg(arguments);
    } else if (strcmp(cmd_name, "history") == 0) {
        cmd_history(arguments);
    } else if (strcmp(cmd_name, "jobs") == 0) {
        cmd_jobs(arguments);
    } else if (strcmp(cmd_name, "pwd") == 0) {
        cmd_pwd(arguments);
    } else if (strcmp(cmd_name, "type") == 0) {
        cmd_type(arguments);
    } else if (strcmp(cmd_name, "unset") == 0) {
        cmd_unset(arguments);
    }
}

__attribute__((noreturn))
static void execute_external(const char *path, PtrArray *arguments) {
    signals_reset();
    ptr_array_append(arguments, NULL);
    execvp(path, (char **)ptr_array_get_c_array(arguments));
    err(EXIT_FAILURE, "execvp");
}

// --- Command infrastructure ---

struct Cmd {
    PtrArray *arguments;
    PtrArray *redirs;
};

static bool apply_redirs(Cmd *cmd) {
    size_t num_redirs = ptr_array_get_size(cmd->redirs);
    for (size_t i = 0; i < num_redirs; i++) {
        if (!redir_do((Redir *)ptr_array_get(cmd->redirs, i))) {
            for (size_t j = 0; j < i; j++) {
                redir_undo((Redir *)ptr_array_get(cmd->redirs, i - 1 - j));
            }
            return false;
        }
    }
    return true;
}

static void undo_redirs(Cmd *cmd) {
    size_t num_redirs = ptr_array_get_size(cmd->redirs);
    for (size_t i = 0; i < num_redirs; i++) {
        redir_undo((Redir *)ptr_array_get(cmd->redirs, num_redirs - 1 - i));
    }
}

// Expand all raw arguments into a new array. Caller owns the result.
static PtrArray *expand_arguments(const PtrArray *raw_args) {
    PtrArray *expanded = ptr_array_create();
    size_t n = ptr_array_get_size(raw_args);
    for (size_t i = 0; i < n; i++) {
        const char *raw = ptr_array_get_const(raw_args, i);
        PtrArray *words = expand_word(raw);
        size_t nw = ptr_array_get_size(words);
        for (size_t j = 0; j < nw; j++) {
            ptr_array_append(expanded, ptr_array_get(words, j));
        }
        ptr_array_destroy_shallow(words);
    }
    return expanded;
}

static void execute(Cmd *cmd, bool fork_on_external) {
    if (ptr_array_get_size(cmd->arguments) == 0) {
        return;
    }

    // Expand arguments at execution time (so $? reflects current state)
    PtrArray *arguments = expand_arguments(cmd->arguments);
    if (ptr_array_get_size(arguments) == 0) {
        ptr_array_destroy(arguments, free);
        return;
    }

    if (!apply_redirs(cmd)) {
        last_exit_status = 1;
        ptr_array_destroy(arguments, free);
        return;
    }

    const char *cmd_name = ptr_array_get(arguments, 0);
    if (is_builtin(cmd_name)) {
        execute_builtin(arguments);
    } else {
        char *path = NULL;
        // If the command contains a slash, use it directly (absolute or relative path)
        if (strchr(cmd_name, '/') != NULL) {
            if (access(cmd_name, X_OK) == 0) {
                path = xstrdup(cmd_name);
            }
        } else {
            path = find_executable(cmd_name);
        }
        if (path == NULL) {
            fprintf(stderr, "%s: command not found\n", cmd_name);
            last_exit_status = 127;
            undo_redirs(cmd);
            ptr_array_destroy(arguments, free);
            return;
        }

        if (!fork_on_external) {
            execute_external(path, arguments);
        }

        pid_t pid = fork();
        if (pid < 0) {
            warn("fork");
            free(path);
            last_exit_status = 1;
            undo_redirs(cmd);
            ptr_array_destroy(arguments, free);
            return;
        }

        if (pid == 0) {
            execute_external(path, arguments);
        }

        int status;
        pid_t wpid = waitpid(pid, &status, 0);
        if (wpid < 0) {
            warn("waitpid");
            last_exit_status = 1;
        } else if (WIFEXITED(status)) {
            last_exit_status = WEXITSTATUS(status);
        } else if (WIFSIGNALED(status)) {
            last_exit_status = 128 + WTERMSIG(status);
        } else {
            last_exit_status = 1;
        }
        free(path);
    }

    undo_redirs(cmd);
    ptr_array_destroy(arguments, free);
}

Cmd *cmd_create(PtrArray *arguments, PtrArray *redirs) {
    Cmd *cmd = xmalloc(sizeof(Cmd));
    cmd->arguments = arguments;
    cmd->redirs = redirs;
    return cmd;
}

void cmd_destroy(void *ptr) {
    if (ptr == NULL) {
        return;
    }
    Cmd *cmd = ptr;
    ptr_array_destroy(cmd->arguments, free);
    ptr_array_destroy(cmd->redirs, redir_destroy);
    free(cmd);
}

static void execute_pipeline(PtrArray *cmds, bool background) {
    size_t num_cmds = ptr_array_get_size(cmds);
    if (num_cmds == 0) {
        return;
    }

    // Single command, not backgrounded — run directly
    if (num_cmds == 1 && !background) {
        execute((Cmd *)ptr_array_get(cmds, 0), true);
        return;
    }

    int fds[2], prev_rfd = -1;
    pid_t pids[num_cmds];

    for (size_t i = 0; i < num_cmds; i++) {
        if (i < num_cmds - 1) {
            if (pipe(fds) < 0) {
                warn("pipe");
                last_exit_status = 1;
                return;
            }
        }

        pid_t pid = fork();
        if (pid < 0) {
            warn("fork");
            if (i < num_cmds - 1) {
                close(fds[0]);
                close(fds[1]);
            }
            last_exit_status = 1;
            return;
        }

        if (pid == 0) {
            signals_reset();
            if (i > 0) {
                dup2(prev_rfd, STDIN_FILENO);
                close(prev_rfd);
            }
            if (i < num_cmds - 1) {
                dup2(fds[1], STDOUT_FILENO);
                close(fds[1]);
                close(fds[0]);
            }

            execute((Cmd *)ptr_array_get(cmds, i), false);
            exit(last_exit_status);
        }

        pids[i] = pid;

        if (i > 0) {
            close(prev_rfd);
        }
        if (i < num_cmds - 1) {
            close(fds[1]);
        }

        prev_rfd = fds[0];
    }

    if (background) {
        // Don't wait — add last process to job table
        // Build command string for display
        job_add(pids[num_cmds - 1], "(pipeline)");
        last_exit_status = 0;
    } else {
        int status;
        for (size_t i = 0; i < num_cmds; i++) {
            waitpid(pids[i], &status, 0);
        }
        if (WIFEXITED(status)) {
            last_exit_status = WEXITSTATUS(status);
        } else if (WIFSIGNALED(status)) {
            last_exit_status = 128 + WTERMSIG(status);
        } else {
            last_exit_status = 1;
        }
    }
}

void execute_ast(AstNode *node) {
    if (node == NULL) {
        return;
    }

    switch (node->type) {
        case NODE_PIPELINE:
            execute_pipeline(node->pipeline, node->background);
            break;

        case NODE_AND:
            execute_ast(node->left);
            if (last_exit_status == 0) {
                execute_ast(node->right);
            }
            break;

        case NODE_OR:
            execute_ast(node->left);
            if (last_exit_status != 0) {
                execute_ast(node->right);
            }
            break;

        case NODE_SEQUENCE:
            execute_ast(node->left);
            execute_ast(node->right);
            break;
    }
}
