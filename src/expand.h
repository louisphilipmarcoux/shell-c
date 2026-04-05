#ifndef SHELL_EXPAND_H_INCLUDED
#define SHELL_EXPAND_H_INCLUDED

#include "ptr_array.h"

// Expands a word: tilde expansion, variable expansion ($VAR, ${VAR}, $?, $$),
// and glob expansion. Returns an array of expanded strings (may be multiple
// due to glob). Caller owns the returned array and its strings.
PtrArray *expand_word(const char *word);

#endif
