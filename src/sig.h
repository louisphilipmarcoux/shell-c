#ifndef SHELL_SIG_H_INCLUDED
#define SHELL_SIG_H_INCLUDED

// Set up signal handlers for the interactive shell.
// SIGINT, SIGTSTP, SIGQUIT ignored in shell; default in children.
void signals_init(void);

// Reset signal handlers to defaults (call after fork, before exec).
void signals_reset(void);

#endif
