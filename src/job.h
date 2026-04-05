#ifndef SHELL_JOB_H_INCLUDED
#define SHELL_JOB_H_INCLUDED

#include <sys/types.h>

typedef enum {
    JOB_RUNNING,
    JOB_STOPPED,
    JOB_DONE,
} JobStatus;

// Add a job to the job table. Returns the job number.
int job_add(pid_t pid, const char *command);

// Update job statuses by reaping terminated/stopped children (non-blocking).
// Prints notifications for completed jobs.
void job_update(void);

// Print all active jobs (the `jobs` builtin).
void job_list(void);

// Bring job n to the foreground.
int job_fg(int n);

// Continue job n in the background.
int job_bg(int n);

#endif
