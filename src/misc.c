#include "misc.h"
#include "ptr_array.h"
#include "xmalloc.h"

#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

const PtrArray *get_all_builtin_names(void) {
    static PtrArray *builtins = NULL;
    if (builtins != NULL) {
        return builtins;
    }

    builtins = ptr_array_create();

    static const char *names[] = {"bg", "cd", "echo", "exit", "export", "fg", "history", "jobs", "pwd", "type", "unset"};
    static size_t num_builtins = sizeof(names) / sizeof(const char *);
    for (size_t i = 0; i < num_builtins; i++) {
        ptr_array_append(builtins, xstrdup(names[i]));
    }

    return builtins;
}

bool is_builtin(const char *cmd_name) {
    const PtrArray *builtins = get_all_builtin_names();
    size_t num_builtins = ptr_array_get_size(builtins);
    for (size_t i = 0; i < num_builtins; i++) {
        if (strcmp(cmd_name, ptr_array_get_const(builtins, i)) == 0) {
            return true;
        }
    }
    return false;
}

static char *path_join(const char *dir, const char *name) {
    size_t size = strlen(dir) + strlen(name) + 2;
    char *path = xmalloc(size);
    snprintf(path, size, "%s/%s", dir, name);
    return path;
}

static bool is_executable(const char *path) {
    struct stat st;
    return stat(path, &st) == 0 && !S_ISDIR(st.st_mode) && access(path, X_OK) == 0;
}

static const PtrArray *split_path_to_dirs(void) {
    static PtrArray *dirs = NULL;
    if (dirs != NULL) {
        return dirs;
    }

    dirs = ptr_array_create();

    const char *path_env = getenv("PATH");
    if (path_env == NULL) {
        return dirs;
    }

    char *path = xstrdup(path_env);
    for (char *dir = strtok(path, ":"); dir != NULL; dir = strtok(NULL, ":")) {
        ptr_array_append(dirs, xstrdup(dir));
    }
    free(path);

    return dirs;
}

char *find_executable(const char *name) {
    const PtrArray *dirs = split_path_to_dirs();
    size_t num_dirs = ptr_array_get_size(dirs);
    for (size_t i = 0; i < num_dirs; i++) {
        const char *dir = ptr_array_get_const(dirs, i);
        char *path = path_join(dir, name);
        if (is_executable(path)) {
            return path;
        }
        free(path);
    }
    return NULL;
}

static void add_executable_names_under_dir(const char *dir, PtrArray *executables) {
    struct dirent **names;
    int num_names = scandir(dir, &names, NULL, alphasort);
    if (num_names < 0) {
        return;
    }

    for (int i = 0; i < num_names; i++) {
        char *path = path_join(dir, names[i]->d_name);
        if (is_executable(path)) {
            ptr_array_append(executables, xstrdup(names[i]->d_name));
        }
        free(path);
        free(names[i]);
    }
    free(names);
}

const PtrArray *get_all_executable_names(void) {
    static PtrArray *executables = NULL;
    if (executables != NULL) {
        return executables;
    }

    executables = ptr_array_create();

    const PtrArray *dirs = split_path_to_dirs();
    size_t num_dirs = ptr_array_get_size(dirs);
    for (size_t i = 0; i < num_dirs; i++) {
        const char *dir = ptr_array_get_const(dirs, i);
        add_executable_names_under_dir(dir, executables);
    }

    return executables;
}

int parse_int(const char *s, int default_val) {
    if (s == NULL || *s == '\0') {
        return default_val;
    }
    errno = 0;
    char *endptr;
    long val = strtol(s, &endptr, 10);
    if (errno != 0 || endptr == s || *endptr != '\0' || val < INT_MIN || val > INT_MAX) {
        return default_val;
    }
    return (int)val;
}
