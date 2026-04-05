#ifndef SHELL_REDIR_H_INCLUDED
#define SHELL_REDIR_H_INCLUDED

#include <stdbool.h>

typedef enum {
    REDIR_INPUT,
    REDIR_NORMAL,
    REDIR_APPEND,
} RedirMode;

typedef struct Redir Redir;

// Allocates memory for an IO redirection.
Redir *redir_create(int fd, const char *path, RedirMode mode);

// Deallocates memory for an IO redirection.
void redir_destroy(void *redir);

// Does an IO redirection. Returns true on success, false on error.
bool redir_do(Redir *redir);

// Undoes an IO redirection.
void redir_undo(Redir *redir);

#endif
