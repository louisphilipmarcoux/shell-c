#ifndef SHELL_CMD_H_INCLUDED
#define SHELL_CMD_H_INCLUDED

#include "ast.h"
#include "ptr_array.h"

typedef struct Cmd Cmd;

// Allocates memory for a command.
Cmd *cmd_create(PtrArray *arguments, PtrArray *redirs);

// Deallocates memory for a command.
void cmd_destroy(void *cmd);

// Executes an AST (pipelines, logical operators, sequences).
void execute_ast(AstNode *node);

// Returns the exit status of the last executed command.
int get_last_exit_status(void);

#endif
