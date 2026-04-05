#ifndef SHELL_AST_H_INCLUDED
#define SHELL_AST_H_INCLUDED

#include <stdbool.h>

#include "ptr_array.h"

typedef enum {
    NODE_PIPELINE,
    NODE_AND,
    NODE_OR,
    NODE_SEQUENCE,
} NodeType;

typedef struct AstNode {
    NodeType type;
    struct AstNode *left;
    struct AstNode *right;
    PtrArray *pipeline;     // Only for NODE_PIPELINE: array of Cmd*
    bool background;        // Run in background (&)
} AstNode;

// Creates a pipeline node owning the given command array.
AstNode *ast_pipeline(PtrArray *cmds, bool background);

// Creates a binary node (AND, OR, SEQUENCE).
AstNode *ast_binary(NodeType type, AstNode *left, AstNode *right);

// Recursively destroys an AST.
void ast_destroy(AstNode *node);

#endif
