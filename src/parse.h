#ifndef SHELL_PARSE_H_INCLUDED
#define SHELL_PARSE_H_INCLUDED

#include "ast.h"
#include "ptr_array.h"

// Parses an array of tokens into an AST representing commands,
// pipelines, logical operators, and sequences.
AstNode *parse(const PtrArray *tokens);

#endif
