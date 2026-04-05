#ifndef SHELL_TOKEN_H_INCLUDED
#define SHELL_TOKEN_H_INCLUDED

typedef enum {
    TOKEN_WORD,
    TOKEN_PIPE,         // |
    TOKEN_OR,           // ||
    TOKEN_AND,          // &&
    TOKEN_SEMICOLON,    // ;
    TOKEN_AMPERSAND,    // &
    TOKEN_IO_NUMBER,
    TOKEN_GREAT,        // >
    TOKEN_DGREAT,       // >>
    TOKEN_LESS,         // <
    TOKEN_DLESS,        // <<
    TOKEN_EOF,
} TokenType;

typedef struct {
    TokenType type;
    char *lexeme;
} Token;

// Allocates memory for a token.
Token *token_create(TokenType type, char *lexeme);

// Deallocates memory for a token.
void token_destroy(void *token);

#endif
