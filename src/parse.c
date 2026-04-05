#include "parse.h"
#include "ast.h"
#include "cmd.h"
#include "ptr_array.h"
#include "redir.h"
#include "token.h"
#include "xmalloc.h"
#include "misc.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static struct {
    const PtrArray *tokens;
    size_t current;
} parser;

static void init(const PtrArray *tokens) {
    parser.tokens = tokens;
    parser.current = 0;
}

static const Token *peek(void) {
    return ptr_array_get_const(parser.tokens, parser.current);
}

static bool check(TokenType type) {
    return peek()->type == type;
}

static bool is_at_end(void) {
    return check(TOKEN_EOF);
}

static const Token *previous(void) {
    return ptr_array_get_const(parser.tokens, parser.current - 1);
}

static const Token *advance(void) {
    if (!is_at_end()) {
        parser.current++;
    }
    return previous();
}

static bool match(TokenType type) {
    if (!check(type)) {
        return false;
    }
    advance();
    return true;
}

static bool is_separator(void) {
    return check(TOKEN_PIPE) || check(TOKEN_AND) || check(TOKEN_OR)
        || check(TOKEN_SEMICOLON) || check(TOKEN_AMPERSAND) || is_at_end();
}

// Parse a single command (arguments + redirections).
// Stores RAW (unexpanded) words — expansion happens at execution time.
static Cmd *command(void) {
    PtrArray *arguments = ptr_array_create();
    PtrArray *redirs = ptr_array_create();

    while (!is_at_end() && !is_separator()) {
        if (match(TOKEN_WORD)) {
            ptr_array_append(arguments, xstrdup(previous()->lexeme));
            continue;
        }

        int fd;
        if (match(TOKEN_IO_NUMBER)) {
            fd = parse_int(previous()->lexeme, -1);
            if (fd < 0) {
                fprintf(stderr, "shell: invalid file descriptor\n");
                break;
            }
        } else {
            fd = -1;
        }

        RedirMode mode;
        if (match(TOKEN_DGREAT)) {
            mode = REDIR_APPEND;
            if (fd < 0) fd = STDOUT_FILENO;
        } else if (match(TOKEN_GREAT)) {
            mode = REDIR_NORMAL;
            if (fd < 0) fd = STDOUT_FILENO;
        } else if (match(TOKEN_LESS)) {
            mode = REDIR_INPUT;
            if (fd < 0) fd = STDIN_FILENO;
        } else if (match(TOKEN_DLESS)) {
            fprintf(stderr, "shell: here-documents not yet supported\n");
            break;
        } else {
            fprintf(stderr, "shell: syntax error near unexpected token '%s'\n", peek()->lexeme);
            advance();
            continue;
        }

        if (is_at_end() || is_separator()) {
            fprintf(stderr, "shell: syntax error: expected filename after redirection\n");
            break;
        }

        ptr_array_append(redirs, redir_create(fd, advance()->lexeme, mode));
    }

    return cmd_create(arguments, redirs);
}

// pipeline -> command ( '|' command )*
static AstNode *pipeline(void) {
    PtrArray *cmds = ptr_array_create();
    ptr_array_append(cmds, command());

    while (match(TOKEN_PIPE)) {
        ptr_array_append(cmds, command());
    }

    bool bg = match(TOKEN_AMPERSAND);
    return ast_pipeline(cmds, bg);
}

// list -> pipeline ( ('&&' | '||') pipeline )*
static AstNode *list(void) {
    AstNode *node = pipeline();

    while (check(TOKEN_AND) || check(TOKEN_OR)) {
        NodeType type = check(TOKEN_AND) ? NODE_AND : NODE_OR;
        advance();
        AstNode *right = pipeline();
        node = ast_binary(type, node, right);
    }

    return node;
}

// command_line -> list ( ';' list )* ';'?
static AstNode *command_line(void) {
    AstNode *node = list();

    while (match(TOKEN_SEMICOLON)) {
        if (is_at_end()) {
            break;
        }
        AstNode *right = list();
        node = ast_binary(NODE_SEQUENCE, node, right);
    }

    return node;
}

AstNode *parse(const PtrArray *tokens) {
    init(tokens);
    if (is_at_end()) {
        return NULL;
    }
    return command_line();
}
