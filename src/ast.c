#include "ast.h"
#include "cmd.h"
#include "xmalloc.h"

#include <stdlib.h>

AstNode *ast_pipeline(PtrArray *cmds, bool background) {
    AstNode *node = xmalloc(sizeof(AstNode));
    node->type = NODE_PIPELINE;
    node->left = NULL;
    node->right = NULL;
    node->pipeline = cmds;
    node->background = background;
    return node;
}

AstNode *ast_binary(NodeType type, AstNode *left, AstNode *right) {
    AstNode *node = xmalloc(sizeof(AstNode));
    node->type = type;
    node->left = left;
    node->right = right;
    node->pipeline = NULL;
    node->background = false;
    return node;
}

void ast_destroy(AstNode *node) {
    if (node == NULL) {
        return;
    }
    ast_destroy(node->left);
    ast_destroy(node->right);
    if (node->pipeline != NULL) {
        ptr_array_destroy(node->pipeline, cmd_destroy);
    }
    free(node);
}
