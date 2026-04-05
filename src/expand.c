#include "expand.h"
#include "cmd.h"
#include "ptr_array.h"
#include "xmalloc.h"

#include <ctype.h>
#include <glob.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// Dynamically growing string buffer.
typedef struct {
    char *data;
    size_t len;
    size_t cap;
} StrBuf;

static void strbuf_init(StrBuf *sb) {
    sb->cap = 64;
    sb->data = xmalloc(sb->cap);
    sb->len = 0;
    sb->data[0] = '\0';
}

static void strbuf_push(StrBuf *sb, char c) {
    if (sb->len + 1 >= sb->cap) {
        sb->cap *= 2;
        sb->data = xrealloc(sb->data, sb->cap);
    }
    sb->data[sb->len++] = c;
    sb->data[sb->len] = '\0';
}

static void strbuf_append(StrBuf *sb, const char *s) {
    while (*s) {
        strbuf_push(sb, *s++);
    }
}

static char *strbuf_finish(StrBuf *sb) {
    return sb->data;
}

// Expand tilde at the start of a word.
static char *expand_tilde(const char *word) {
    if (word[0] != '~') {
        return xstrdup(word);
    }

    const char *rest = word + 1;
    if (*rest == '/' || *rest == '\0') {
        const char *home = getenv("HOME");
        if (home == NULL) {
            return xstrdup(word);
        }
        StrBuf sb;
        strbuf_init(&sb);
        strbuf_append(&sb, home);
        strbuf_append(&sb, rest);
        return strbuf_finish(&sb);
    }

    // ~user form
    const char *slash = strchr(rest, '/');
    char *username;
    if (slash != NULL) {
        username = xstrndup(rest, slash - rest);
    } else {
        username = xstrdup(rest);
        slash = rest + strlen(rest);
    }

    struct passwd *pw = getpwnam(username);
    free(username);
    if (pw == NULL) {
        return xstrdup(word);
    }

    StrBuf sb;
    strbuf_init(&sb);
    strbuf_append(&sb, pw->pw_dir);
    strbuf_append(&sb, slash);
    return strbuf_finish(&sb);
}

// Expand environment variables in a string.
// Handles $VAR, ${VAR}, ${VAR:-default}, $?, $$
static char *expand_variables(const char *word) {
    StrBuf sb;
    strbuf_init(&sb);

    bool in_single_quote = false;
    bool in_double_quote = false;

    for (const char *p = word; *p != '\0'; p++) {
        if (*p == '\'' && !in_double_quote) {
            in_single_quote = !in_single_quote;
            strbuf_push(&sb, *p);
            continue;
        }
        if (*p == '"' && !in_single_quote) {
            in_double_quote = !in_double_quote;
            strbuf_push(&sb, *p);
            continue;
        }

        if (*p == '\\' && !in_single_quote) {
            strbuf_push(&sb, *p);
            if (*(p + 1) != '\0') {
                p++;
                strbuf_push(&sb, *p);
            }
            continue;
        }

        if (*p != '$' || in_single_quote) {
            strbuf_push(&sb, *p);
            continue;
        }

        // Handle $ expansions
        p++;
        if (*p == '?') {
            char buf[16];
            snprintf(buf, sizeof(buf), "%d", get_last_exit_status());
            strbuf_append(&sb, buf);
        } else if (*p == '$') {
            char buf[16];
            snprintf(buf, sizeof(buf), "%d", (int)getpid());
            strbuf_append(&sb, buf);
        } else if (*p == '{') {
            p++;
            const char *start = p;
            // Look for :- for default value
            const char *colon_dash = NULL;
            while (*p != '\0' && *p != '}') {
                if (*p == ':' && *(p + 1) == '-') {
                    colon_dash = p;
                }
                p++;
            }
            if (*p == '\0') {
                strbuf_append(&sb, "${");
                p = start - 1;
                continue;
            }

            if (colon_dash != NULL) {
                char *varname = xstrndup(start, colon_dash - start);
                char *defval = xstrndup(colon_dash + 2, p - (colon_dash + 2));
                const char *val = getenv(varname);
                if (val != NULL && *val != '\0') {
                    strbuf_append(&sb, val);
                } else {
                    strbuf_append(&sb, defval);
                }
                free(varname);
                free(defval);
            } else {
                char *varname = xstrndup(start, p - start);
                const char *val = getenv(varname);
                if (val != NULL) {
                    strbuf_append(&sb, val);
                }
                free(varname);
            }
        } else if (isalpha(*p) || *p == '_') {
            const char *start = p;
            while (isalnum(*p) || *p == '_') {
                p++;
            }
            char *varname = xstrndup(start, p - start);
            const char *val = getenv(varname);
            if (val != NULL) {
                strbuf_append(&sb, val);
            }
            free(varname);
            p--; // Loop will increment
        } else {
            strbuf_push(&sb, '$');
            p--; // Re-examine this character
        }
    }

    return strbuf_finish(&sb);
}

// Perform glob expansion on a word. Returns array of results.
static PtrArray *expand_glob(const char *word) {
    PtrArray *results = ptr_array_create();

    glob_t gl;
    int ret = glob(word, GLOB_NOCHECK | GLOB_TILDE, NULL, &gl);
    if (ret == 0) {
        for (size_t i = 0; i < gl.gl_pathc; i++) {
            ptr_array_append(results, xstrdup(gl.gl_pathv[i]));
        }
        globfree(&gl);
    } else {
        ptr_array_append(results, xstrdup(word));
    }

    return results;
}

// Strip quotes from a word (single and double quotes, handle escapes).
static char *strip_quotes(const char *word) {
    StrBuf sb;
    strbuf_init(&sb);

    bool in_single_quote = false;
    bool in_double_quote = false;

    for (const char *p = word; *p != '\0'; p++) {
        if (*p == '\'' && !in_double_quote) {
            in_single_quote = !in_single_quote;
            continue;
        }
        if (*p == '"' && !in_single_quote) {
            in_double_quote = !in_double_quote;
            continue;
        }
        if (*p == '\\' && !in_single_quote) {
            if (*(p + 1) != '\0') {
                p++;
                if (in_double_quote) {
                    // In double quotes, only \$, \`, \\, \", \newline are special
                    if (*p == '$' || *p == '`' || *p == '\\' || *p == '"' || *p == '\n') {
                        strbuf_push(&sb, *p);
                    } else {
                        strbuf_push(&sb, '\\');
                        strbuf_push(&sb, *p);
                    }
                } else {
                    strbuf_push(&sb, *p);
                }
                continue;
            }
        }
        strbuf_push(&sb, *p);
    }

    return strbuf_finish(&sb);
}

// Check if word contains unquoted glob characters.
static bool has_glob_chars(const char *word) {
    bool in_single_quote = false;
    bool in_double_quote = false;

    for (const char *p = word; *p != '\0'; p++) {
        if (*p == '\'' && !in_double_quote) {
            in_single_quote = !in_single_quote;
            continue;
        }
        if (*p == '"' && !in_single_quote) {
            in_double_quote = !in_double_quote;
            continue;
        }
        if (*p == '\\' && !in_single_quote) {
            if (*(p + 1) != '\0') p++;
            continue;
        }
        if (!in_single_quote && !in_double_quote) {
            if (*p == '*' || *p == '?' || *p == '[') {
                return true;
            }
        }
    }
    return false;
}

PtrArray *expand_word(const char *word) {
    // 1. Tilde expansion
    char *tilde_expanded = expand_tilde(word);

    // 2. Variable expansion
    char *var_expanded = expand_variables(tilde_expanded);
    free(tilde_expanded);

    // 3. Glob expansion (only if there are unquoted glob chars)
    PtrArray *results;
    if (has_glob_chars(var_expanded)) {
        // Strip quotes for glob, then glob
        char *stripped = strip_quotes(var_expanded);
        results = expand_glob(stripped);
        free(stripped);
    } else {
        // Just strip quotes
        results = ptr_array_create();
        char *stripped = strip_quotes(var_expanded);
        ptr_array_append(results, stripped);
    }

    free(var_expanded);
    return results;
}
