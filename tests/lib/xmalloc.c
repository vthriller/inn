/* $Id$ */
/* Test suite for xmalloc and family. */

#include "config.h"
#include "clibrary.h"
#include <ctype.h>
#include <errno.h>

#include "libinn.h"

/* A customized error handler for checking xmalloc's support of them.
   Prints out the error message and exits with status 1. */
static void
test_handler(const char *function, size_t size, const char *file, int line)
{
    die("%s %lu %s %d", function, (unsigned long) size, file, line);
}

/* Allocate the amount of memory given and write to all of it to make sure
   we can, returning true if that succeeded and false on any sort of
   detectable error. */
static int
test_malloc(size_t size)
{
    char *buffer;
    size_t i;

    buffer = xmalloc(size);
    if (!buffer) return 0;
    if (size > 0) memset(buffer, 1, size);
    for (i = 0; i < size; i++)
        if (buffer[i] != 1) return 0;
    free(buffer);
    return 1;
}

/* Allocate half the memory given, write to it, then reallocate to the
   desired size, writing to the rest and then checking it all.  Returns true
   on success, false on any failure. */
static int
test_realloc(size_t size)
{
    char *buffer;
    size_t i;

    buffer = xmalloc(size / 2);
    if (!buffer) return 0;
    if (size / 2 > 0) memset(buffer, 1, size / 2);
    buffer = xrealloc(buffer, size);
    if (!buffer) return 0;
    if (size > 0) memset(buffer + size / 2, 2, size - size / 2);
    for (i = 0; i < size / 2; i++)
        if (buffer[i] != 1) return 0;
    for (i = size / 2; i < size; i++)
        if (buffer[i] != 2) return 0;
    free(buffer);
    return 1;
}

/* Generate a string of the size indicated, call xstrdup on it, and then
   ensure the result matches.  Returns true on success, false on any
   failure. */
static int
test_strdup(size_t size)
{
    char *string, *copy;
    int match;

    string = xmalloc(size);
    if (!string) return 0;
    memset(string, 1, size - 1);
    string[size - 1] = '\0';
    copy = xstrdup(string);
    if (!copy) return 0;
    match = strcmp(string, copy);
    free(string);
    free(copy);
    return (match == 0);
}

/* Generate a string of the size indicated plus some, call xstrndup on it, and
   then ensure the result matches.  Returns true on success, false on any
   failure. */
static int
test_strndup(size_t size)
{
    char *string, *copy;
    int match, toomuch;

    string = xmalloc(size + 1);
    if (!string) return 0;
    memset(string, 1, size - 1);
    string[size - 1] = 2;
    string[size] = '\0';
    copy = xstrndup(string, size - 1);
    if (!copy) return 0;
    match = strncmp(string, copy, size - 1);
    toomuch = strcmp(string, copy);
    free(string);
    free(copy);
    return (match == 0 && toomuch != 0);
}

/* Allocate the amount of memory given and check that it's all zeroed,
   returning true if that succeeded and false on any sort of detectable
   error. */
static int
test_calloc(size_t size)
{
    char *buffer;
    size_t i, nelems;

    nelems = size / 4;
    if (nelems * 4 != size)
        return 0;
    buffer = xcalloc(nelems, 4);
    if (!buffer)
        return 0;
    for (i = 0; i < size; i++)
        if (buffer[i] != 0)
            return 0;
    free(buffer);
    return 1;
}

/* Take the amount of memory to allocate in bytes as a command-line argument
   and call test_malloc with that amount of memory. */
int
main(int argc, char *argv[])
{
    size_t size;
    unsigned char code;

    if (argc < 2) die("Usage error.  Both type and size must be given.");
    errno = 0;
    size = strtol(argv[2], 0, 10);
    if (size == 0 && errno != 0) sysdie("Invalid size");

    /* If the code is capitalized, install our customized error handler. */
    code = argv[1][0];
    if (isupper(code)) {
        xmalloc_error_handler = test_handler;
        code = tolower(code);
    }

    switch (code) {
    case 'c': exit(test_calloc(size) ? 0 : 1);
    case 'm': exit(test_malloc(size) ? 0 : 1);
    case 'r': exit(test_realloc(size) ? 0 : 1);
    case 's': exit(test_strdup(size) ? 0 : 1);
    case 'n': exit(test_strndup(size) ? 0 : 1);
    default:
        die("Unknown mode %c", argv[1][0]);
        break;
    }
    exit(1);
}
