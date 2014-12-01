/*  $Id$
**
**  Here be declarations of routines and variables in the C library.
**  Including this file is the equivalent of including all of the following
**  headers, portably:
**
**      #include "inn/macros.h"
**      #include <limits.h>
**      #include <sys/types.h>
**      #include <stdarg.h>
**      #include <stdio.h>
**      #include <stdlib.h>
**      #include <stddef.h>
**      #include <stdint.h>
**      #include <string.h>
**      #include <unistd.h>
**
**  Missing functions are provided via #define or prototyped if we'll be
**  adding them to INN's library.  If the system doesn't define a SUN_LEN
**  macro, one will be provided.  Also provides some standard #defines.
*/

#ifndef CLIBRARY_H
#define CLIBRARY_H 1

/* Make sure we have our configuration information. */
#include "config.h"
#include "inn/macros.h"

/* Assume stdarg is available; don't bother with varargs support any more.
   We need this to be able to declare vsnprintf. */
#include <stdarg.h>

/* This is the same method used by autoconf as of 2000-07-29 for including
   the basic system headers with the addition of handling of strchr,
   strrchr, and memcpy.  Note that we don't attempt to declare any of the
   functions; the number of systems left without ANSI-compatible function
   prototypes isn't high enough to be worth the trouble.  */
#include <limits.h>
#include <stdio.h>
#include <sys/types.h>
#include <stdlib.h>
#include <stddef.h>
#if HAVE_STRING_H
# include <string.h>
#endif
#if HAVE_STRINGS_H
# include <strings.h>
#endif

#if HAVE_INTTYPES_H
# include <inttypes.h>
#endif
#if HAVE_STDINT_H
# include <stdint.h>
#endif
#if HAVE_UNISTD_H
# include <unistd.h>
#endif

/* SCO OpenServer gets int32_t from here. */
#if HAVE_SYS_BITYPES_H
# include <sys/bitypes.h>
#endif

BEGIN_DECLS

/* Handle defining fseeko and ftello.  If HAVE_FSEEKO is defined, the system
   header files take care of this for us.  Otherwise, see if we're building
   with large file support.  If we are, we have to provide some real fseeko
   and ftello implementation; declare them if they're not already declared and
   we'll use replacement versions in libinn.  If we're not using large files,
   we can safely just use fseek and ftell.

   We'd rather use fseeko and ftello unconditionally, even when we're not
   building with large file support, since they're a better interface.
   Unfortunately, they're available but not declared on some systems unless
   building with large file support, the AC_FUNC_FSEEKO Autoconf function
   always turns on large file support, and our fake declarations won't work on
   some systems (like HP_UX).  This is the best compromise we've been able to
   come up with. */
#if !HAVE_FSEEKO
# if DO_LARGEFILES
#  if !HAVE_DECL_FSEEKO
extern int              fseeko(FILE *, off_t, int);
#  endif
#  if !HAVE_DECL_FTELLO
extern off_t            ftello(FILE *);
#  endif
# else
#  define fseeko(f, o, w) fseek((f), (long)(o), (w))
#  define ftello(f)       ftell(f)
# endif
#endif

/* Provide prototypes for functions not declared in system headers.  Use the
   HAVE_DECL macros for those functions that may be prototyped but
   implemented incorrectly or implemented without a prototype. */
#if !HAVE_ASPRINTF
extern int              asprintf(char **, const char *, ...)
    __attribute__((__format__(printf, 2, 3)));
extern int              vasprintf(char **, const char *, va_list);
#endif
#if !HAVE_MKSTEMP
extern int              mkstemp(char *);
#endif
#if !HAVE_DECL_PREAD
extern ssize_t          pread(int, void *, size_t, off_t);
#endif
#if !HAVE_DECL_PWRITE
extern ssize_t          pwrite(int, const void *, size_t, off_t);
#endif
#if !HAVE_REALLOCARRAY
extern void             *reallocarray(void *, size_t, size_t);
#endif
#if !HAVE_SETENV
extern int              setenv(const char *, const char *, int);
#endif
#if !HAVE_SETEUID
extern int              seteuid(uid_t);
#endif
#if !HAVE_DECL_SNPRINTF
extern int              snprintf(char *, size_t, const char *, ...)
    __attribute__((__format__(printf, 3, 4)));
#endif
#if !HAVE_DECL_STRLCAT
extern size_t           strlcat(char *, const char *, size_t);
#endif
#if !HAVE_DECL_STRLCPY
extern size_t           strlcpy(char *, const char *, size_t);
#endif
#if !HAVE_SYMLINK
extern int              symlink(const char *, const char *);
#endif
#if !HAVE_DECL_VSNPRINTF
extern int              vsnprintf(char *, size_t, const char *, va_list);
#endif

/* In case <sys/types.h> does not define ptrdiff_t. */
#if !HAVE_PTRDIFF_T
typedef long            ptrdiff_t;
#endif
/* In case <signal.h> does not define sig_atomic_t. */
#if !HAVE_SIG_ATOMIC_T
typedef int             sig_atomic_t;
#endif

END_DECLS

/* "Good enough" replacements for standard functions. */
#if !HAVE_ATEXIT
# define atexit(arg) on_exit((arg), 0)
#endif
#if !HAVE_STRTOUL
# define strtoul(a, b, c) (unsigned long) strtol((a), (b), (c))
#endif

/* This almost certainly isn't necessary, but it's not hurting anything.
   gcc assumes that if SEEK_SET isn't defined none of the rest are either,
   so we certainly can as well. */
#ifndef SEEK_SET
# define SEEK_SET 0
# define SEEK_CUR 1
# define SEEK_END 2
#endif

/* POSIX requires that these be defined in <unistd.h>.  If one of them has
   been defined, all the rest almost certainly have. */
#ifndef STDIN_FILENO
# define STDIN_FILENO   0
# define STDOUT_FILENO  1
# define STDERR_FILENO  2
#endif

/* POSIX.1g requires <sys/un.h> to define a SUN_LEN macro for determining
   the real length of a struct sockaddr_un, but it's not available
   everywhere yet.  If autoconf couldn't find it, define our own.  This
   definition is from 4.4BSD by way of Stevens, Unix Network Programming
   (2nd edition), vol. 1, pg. 917. */
#if !HAVE_SUN_LEN
# define SUN_LEN(sun) \
    (sizeof(*(sun)) - sizeof((sun)->sun_path) + strlen((sun)->sun_path))
#endif

/* C99 requires va_copy.  Older versions of GCC provide __va_copy.  Per the
   Autoconf manual, memcpy is a generally portable fallback. */
#ifndef va_copy
# ifdef __va_copy
#  define va_copy(d, s)         __va_copy((d), (s))
# else
#  define va_copy(d, s)         memcpy(&(d), &(s), sizeof(va_list))
# endif
#endif

#endif /* !CLIBRARY_H */
