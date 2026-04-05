#ifndef SHELL_SCAN_H_INCLUDED
#define SHELL_SCAN_H_INCLUDED

#include "ptr_array.h"

// Tokenizes a line.
PtrArray *scan(const char *line);

#endif
