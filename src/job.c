#include "job.h"
#include "xmalloc.h"

#include <err.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define MAX_JOBS 64

typedef struct {
    pid_t pid;
    char *command;
    JobStatus status;
    bool active;
} Job;

static Job jobs[MAX_JOBS];
static int num_jobs = 0;

int job_add(pid_t pid, const char *command) {
    for (int i = 0; i < MAX_JOBS; i++) {
        if (!jobs[i].active) {
            jobs[i].pid = pid;
            jobs[i].command = xstrdup(command);
            jobs[i].status = JOB_RUNNING;
            jobs[i].active = true;
            if (i >= num_jobs) {
                num_jobs = i + 1;
            }
            fprintf(stderr, "[%d] %d\n", i + 1, pid);
            return i + 1;
        }
    }
    fprintf(stderr, "shell: too many background jobs\n");
    return -1;
}

void job_update(void) {
    int status;
    pid_t pid;

    while ((pid = waitpid(-1, &status, WNOHANG | WUNTRACED)) > 0) {
        for (int i = 0; i < num_jobs; i++) {
            if (jobs[i].active && jobs[i].pid == pid) {
                if (WIFEXITED(status) || WIFSIGNALED(status)) {
                    fprintf(stderr, "[%d]  Done                    %s\n", i + 1, jobs[i].command);
                    free(jobs[i].command);
                    jobs[i].command = NULL;
                    jobs[i].active = false;
                } else if (WIFSTOPPED(status)) {
                    jobs[i].status = JOB_STOPPED;
                    fprintf(stderr, "[%d]  Stopped                 %s\n", i + 1, jobs[i].command);
                }
                break;
            }
        }
    }
}

void job_list(void) {
    for (int i = 0; i < num_jobs; i++) {
        if (jobs[i].active) {
            const char *status_str = jobs[i].status == JOB_RUNNING ? "Running" : "Stopped";
            printf("[%d]  %-24s%s\n", i + 1, status_str, jobs[i].command);
        }
    }
}

int job_fg(int n) {
    int idx = n - 1;
    if (idx < 0 || idx >= num_jobs || !jobs[idx].active) {
        fprintf(stderr, "fg: %%%d: no such job\n", n);
        return 1;
    }

    fprintf(stderr, "%s\n", jobs[idx].command);

    // Continue the process if stopped
    if (jobs[idx].status == JOB_STOPPED) {
        kill(jobs[idx].pid, SIGCONT);
        jobs[idx].status = JOB_RUNNING;
    }

    // Wait for it
    int status;
    waitpid(jobs[idx].pid, &status, WUNTRACED);

    if (WIFSTOPPED(status)) {
        jobs[idx].status = JOB_STOPPED;
        fprintf(stderr, "\n[%d]  Stopped                 %s\n", n, jobs[idx].command);
        return 128 + WSTOPSIG(status);
    }

    // Job completed
    free(jobs[idx].command);
    jobs[idx].command = NULL;
    jobs[idx].active = false;

    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }
    return 1;
}

int job_bg(int n) {
    int idx = n - 1;
    if (idx < 0 || idx >= num_jobs || !jobs[idx].active) {
        fprintf(stderr, "bg: %%%d: no such job\n", n);
        return 1;
    }

    if (jobs[idx].status != JOB_STOPPED) {
        fprintf(stderr, "bg: job %d already running\n", n);
        return 1;
    }

    jobs[idx].status = JOB_RUNNING;
    fprintf(stderr, "[%d]  %s &\n", n, jobs[idx].command);
    kill(jobs[idx].pid, SIGCONT);
    return 0;
}
