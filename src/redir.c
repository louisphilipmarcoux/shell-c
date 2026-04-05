#include "redir.h"
#include "xmalloc.h"

#include <err.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>

struct Redir {
    int fd, saved_fd;
    char *path;
    RedirMode mode;
};

Redir *redir_create(int fd, const char *path, RedirMode mode) {
    Redir *redir = xmalloc(sizeof(Redir));
    redir->fd = fd;
    redir->saved_fd = -1;
    redir->path = xstrdup(path);
    redir->mode = mode;
    return redir;
}

void redir_destroy(void *ptr) {
    if (ptr == NULL) {
        return;
    }
    Redir *redir = ptr;
    free(redir->path);
    free(redir);
}

bool redir_do(Redir *redir) {
    int open_flags;
    if (redir->mode == REDIR_INPUT) {
        open_flags = O_RDONLY;
    } else {
        open_flags = O_WRONLY | O_CREAT | (redir->mode == REDIR_APPEND ? O_APPEND : O_TRUNC);
    }

    int file_fd = open(redir->path, open_flags, 0644);
    if (file_fd < 0) {
        warn("%s", redir->path);
        return false;
    }

    redir->saved_fd = dup(redir->fd);
    if (redir->saved_fd < 0) {
        warn("dup");
        close(file_fd);
        return false;
    }

    if (dup2(file_fd, redir->fd) < 0) {
        warn("dup2");
        close(file_fd);
        close(redir->saved_fd);
        redir->saved_fd = -1;
        return false;
    }

    close(file_fd);
    return true;
}

void redir_undo(Redir *redir) {
    if (redir->saved_fd < 0) {
        return;
    }
    fsync(redir->fd);
    dup2(redir->saved_fd, redir->fd);
    close(redir->saved_fd);
    redir->saved_fd = -1;
}
