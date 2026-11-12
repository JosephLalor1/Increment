// Which concepts a piece of C or C++ uses, read from its tokens. See code_skills.h.
// This only says what the code contains; earned XP and answer history live elsewhere.
#include "code/code_skills.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

// A word, number or operator in the source: a slice of it, not a copy.
typedef struct {
    const char *start;
    size_t length;
} Token;

static bool Is(Token t, const char *word) { return t.length == strlen(word) && !strncmp(t.start, word, t.length); }
static bool Identifier(Token t) { return t.length && (isalpha((unsigned char)*t.start) || *t.start == '_'); }

// Whether the token is a built-in type name.
static bool Type(Token t)
{
    const char *types[] = { "int",      "char",   "float", "double", "long",   "short",
                            "unsigned", "signed", "bool",  "void",   "size_t", "auto" };
    for (unsigned i = 0; i < sizeof types / sizeof *types; i++)
        if (Is(t, types[i])) return true;
    return false;
}

CodeSkills CodeSkillsDetect(const char *source, bool cpp)
{
    CodeSkills result = { 0 };
    if (!source) return result;
    size_t length = strlen(source), count = 0;
    Token *tokens = calloc(length + 1, sizeof *tokens);
    if (!tokens) return result;
    const char *p = source;

    // Split the source into tokens, leaving out everything that isn't code: preprocessor
    // lines (with their continuations), comments, raw strings, strings and characters.
    while (*p) {
        if (isspace((unsigned char)*p)) {
            p++;
            continue;
        }
        if (*p == '#') {
            do {
                while (*p && *p != '\n') p++;
                if (p > source && p[-1] == '\\' && *p)
                    p++;
                else
                    break;
            } while (*p);
            continue;
        }
        if (p[0] == '/' && p[1] == '/') {
            while (*p && *p != '\n') p++;
            continue;
        }
        if (p[0] == '/' && p[1] == '*') {
            const char *end = strstr(p + 2, "*/");
            p = end ? end + 2 : p + strlen(p);
            continue;
        }
        if (cpp && p[0] == 'R' && p[1] == '"') {
            const char *open = strchr(p + 2, '(');
            if (open && open - (p + 2) <= 16) {
                char closing[20];
                size_t n = (size_t)(open - (p + 2));
                closing[0] = ')';
                memcpy(closing + 1, p + 2, n);
                closing[n + 1] = '"';
                closing[n + 2] = 0;
                const char *end = strstr(open + 1, closing);
                p = end ? end + n + 2 : p + strlen(p);
                continue;
            }
        }
        if (*p == '"' || *p == '\'') {
            char quote = *p++;
            while (*p && *p != quote) {
                if (*p == '\\' && p[1]) p++;
                p++;
            }
            if (*p) p++;
            continue;
        }
        // A name or number runs to its last word character; an operator is one character,
        // or two for :: -> ++ -- && || == != <= >= += -= and the like.
        const char *start = p++;
        if (isalnum((unsigned char)*start) || *start == '_')
            while (isalnum((unsigned char)*p) || *p == '_') p++;
        else if (*p &&
                 ((start[0] == ':' && *p == ':') || (start[0] == '-' && *p == '>') ||
                  (strchr("+-&|<>=!", start[0]) && (*p == '=' || *p == start[0]))))
            p++;
        tokens[count++] = (Token){ start, (size_t)(p - start) };
    }

    // Look at each token with its neighbours. A concept is present as soon as one telltale
    // token is found; nothing is ever taken away.
    Token empty = { "", 0 };
    Token userTypes[128];
    size_t typeCount = 0;
    for (size_t i = 0; i < count; i++) {
        Token t = tokens[i], prev = i ? tokens[i - 1] : empty, next = i + 1 < count ? tokens[i + 1] : empty;
        if ((Is(t, "struct") || (cpp && Is(t, "class"))) && Identifier(next) && typeCount < 128)
            userTypes[typeCount++] = next;
        if (Type(t)) result.present[SKILL_TYPES] = true;
        if (Is(t, "for") || Is(t, "while") || Is(t, "do")) result.present[SKILL_LOOPS] = true;
        if (Is(t, "if") || Is(t, "else") || Is(t, "switch") || Is(t, "case") || Is(t, "?"))
            result.present[SKILL_CONDITIONS] = true;
        if (Is(t, "[") && (Identifier(prev) || Is(prev, "]"))) result.present[SKILL_ARRAYS] = true;
        if (Is(t, "printf") || Is(t, "puts") || Is(t, "putchar") || Is(t, "fprintf") ||
            (cpp && (Is(t, "cout") || Is(t, "cerr"))))
            result.present[SKILL_OUTPUT] = true;
        if (Is(t, "scanf") || Is(t, "fgets") || Is(t, "getchar") || (cpp && (Is(t, "cin") || Is(t, "getline"))))
            result.present[SKILL_INPUT] = true;
        if (Is(t, "strlen") || Is(t, "strcmp") || Is(t, "strcpy") || Is(t, "strcat") || (cpp && Is(t, "string")))
            result.present[SKILL_STRINGS] = true;
        if (Is(t, "malloc") || Is(t, "calloc") || Is(t, "realloc") || Is(t, "free") ||
            (cpp && (Is(t, "new") || Is(t, "delete"))))
            result.present[SKILL_DYNAMIC_MEMORY] = true;
        if (cpp && (Is(t, "class") || Is(t, "struct"))) result.present[SKILL_CLASSES] = true;
        if (cpp && (Is(t, "public") || Is(t, "private") || Is(t, "protected")))
            result.present[SKILL_ENCAPSULATION] = true;
        if (cpp &&
            (Is(t, "::") || Is(t, "namespace") || Is(t, "cout") || Is(t, "cin") || Is(t, "class") || Is(t, "new") ||
             Is(t, "delete")))
            result.present[SKILL_CPP_BASICS] = true;
        if (cpp && (Is(t, "vector") || Is(t, "array"))) {
            result.present[SKILL_ARRAYS] = true;
            result.present[SKILL_CPP_BASICS] = true;
        }
        if (Is(t, "+") || Is(t, "-") || Is(t, "/") || Is(t, "%") || Is(t, "++") || Is(t, "--") || Is(t, "+=") ||
            Is(t, "-="))
            result.present[SKILL_OPERATORS] = true;
        // A '*' or '&' after a type (built in, or a struct or class named earlier) declares a
        // pointer or reference; anywhere else it is arithmetic or an address.
        bool declaredType = Type(prev);
        if (!declaredType && (Is(t, "*") || Is(t, "&") || Is(t, "&&")))
            for (size_t j = 0; j < typeCount; j++)
                if (userTypes[j].length == prev.length && !strncmp(userTypes[j].start, prev.start, prev.length))
                    declaredType = true;
        if (Is(t, "*") && (declaredType || Is(prev, "*"))) {
            result.present[SKILL_POINTERS] = true;
            if (Is(prev, "*")) result.present[SKILL_INDIRECTION] = true;
        }
        if (cpp && (Is(t, "&") || Is(t, "&&")) && declaredType && Identifier(next))
            result.present[SKILL_REFERENCES] = true;
        if (Is(t, "->")) result.present[SKILL_POINTERS] = true;
        if (Is(t, "(") && Is(next, "*") && i + 4 < count && Is(tokens[i + 3], ")") && Is(tokens[i + 4], "(")) {
            result.present[SKILL_FUNCTION_POINTERS] = result.present[SKILL_POINTERS] = true;
        }
        // "type name(...) {" defines a function; main only shows the program's structure.
        if (Identifier(t) && Is(next, "(") && (Type(prev) || Is(prev, "*"))) {
            size_t j = i + 2;
            int depth = 1;
            for (; j < count && depth; j++) {
                if (Is(tokens[j], "(")) depth++;
                if (Is(tokens[j], ")")) depth--;
            }
            if (j < count && Is(tokens[j], "{"))
                result.present[Is(t, "main") ? SKILL_PROGRAM_STRUCTURE : SKILL_FUNCTIONS] = true;
        }
    }
    free(tokens);
    result.complete = true;
    return result;
}
