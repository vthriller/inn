/*  $Id$
**
**  Replacement for a missing getaddrinfo.
**
**  Written by Russ Allbery <rra@stanford.edu>
**  This work is hereby placed in the public domain by its author.
**
**  This is an implementation of getaddrinfo for systems that don't have one
**  so that networking code can use a consistant interface without #ifdef.  It
**  is a fairly minimal implementation, with the following limitations:
**
**    - IPv4 support only.  IPv6 is not supported.
**    - AI_ADDRCONFIG is ignored.
**    - Not thread-safe due to gethostbyname and getservbyname.
**    - SOCK_DGRAM and SOCK_STREAM only.
**    - Multiple possible socket types only generate one addrinfo struct.
**
**  The last three issues could probably be easily remedied, but weren't
**  needed for INN's purposes.  Adding IPv6 support isn't worth it; systems
**  with IPv6 support should already support getaddrinfo natively.
*/

#include "config.h"
#include "clibrary.h"
#include "portable/socket.h"
#include <errno.h>

/* If we're running the test suite, rename inet_ntoa to avoid conflicts with
   the system version.  Note that we don't rename the structures and
   constants, but that should be okay (except possibly for gai_strerror. */
#if TESTING
# define gai_strerror test_gai_strerror
# define freeaddrinfo test_freeaddrinfo
# define getaddrinfo  test_getaddrinfo
const char *test_gai_strerror(int);
void test_freeaddrinfo(struct addrinfo *);
int test_getaddrinfo(const char *, const char *, const struct addrinfo *,
                     struct addrinfo **);

/* Make sure that AI_NUMERICSERV is set to something useful even on platforms
   that don't support it when testing. */
# if AI_NUMERICSERV == 0
#  undef AI_NUMERICSERV
#  define AI_NUMERICSERV 0x0080
# endif
#endif


/* Table of strings corresponding to the EAI_* error codes. */
static const char * const gai_errors[] = {
    "Host name lookup failure",         /* 1 EAI_AGAIN */
    "Invalid flag value",               /* 2 EAI_BADFLAGS */
    "Unknown server error",             /* 3 EAI_FAIL */
    "Unsupported address family",       /* 4 EAI_FAMILY */
    "Memory allocation failure",        /* 5 EAI_MEMORY */
    "Host unknown or not given",        /* 6 EAI_NONAME */
    "Service not supported for socket", /* 7 EAI_SERVICE */
    "Unsupported socket type",          /* 8 EAI_SOCKTYPE */
    "System error"                      /* 9 EAI_SYSTEM */
};

/* Value representing all of the hint flags set. */
#if TESTING
# define AI_INTERNAL_ALL 0x008f
#else
# define AI_INTERNAL_ALL 0x004f
#endif

/* Macro to set the len attribute of sockaddr_in. */
#if HAVE_SOCKADDR_LEN
# define sin_set_length(s)      ((s)->sin_len  = sizeof(struct sockaddr_in))
#else
# define sin_set_length(s)      /* empty */
#endif


/*
**  Return a constant string for a given EAI_* error code or a string
**  indicating an unknown error.
*/
const char *
gai_strerror(int ecode)
{
    if (ecode < 1 || (size_t) ecode > ARRAY_SIZE(gai_errors))
        return "Unknown error";
    else
        return gai_errors[ecode - 1];
}


/*
**  Free a linked list of addrinfo structs.
*/
void
freeaddrinfo(struct addrinfo *ai)
{
    struct addrinfo *next;

    while (ai != NULL) {
        next = ai->ai_next;
        if (ai->ai_addr != NULL)
            free(ai->ai_addr);
        if (ai->ai_canonname != NULL)
            free(ai->ai_canonname);
        free(ai);
        ai = next;
    }
}


/*
**  Convert a numeric service string to a number with error checking,
**  returning true if the number was parsed correctly and false otherwise.
**  Stores the converted number in the second argument.  Equivalent to calling
**  strtol, but with the base always fixed at 10, with checking of errno,
**  ensuring that all of the string is consumed, and checking that the
**  resulting number is positive.
*/
static bool
convert_service(const char *string, long *result)
{
    char *end;

    if (*string == '\0')
        return false;
    errno = 0;
    *result = strtol(string, &end, 10);
    if (errno != 0 || *end != '\0' || *result < 0)
        return false;
    return true;
}


/*
**  Allocate a new addrinfo struct, setting some defaults given that this
**  implementation is IPv4 only.  Also allocates an attached sockaddr_in and
**  zeroes it, per the requirement for getaddrinfo.  Takes the socktype,
**  canonical name (which is copied if not NULL), address, and port.  Returns
**  NULL on a memory allocation failure.
*/
static struct addrinfo *
gai_addrinfo_new(int socktype, const char *canonical, struct in_addr addr,
                 unsigned short port)
{
    struct addrinfo *ai;

    ai = malloc(sizeof(*ai));
    if (ai == NULL)
        return NULL;
    ai->ai_addr = malloc(sizeof(struct sockaddr_in));
    if (ai->ai_addr == NULL) {
        free(ai);
        return NULL;
    }
    ai->ai_next = NULL;
    if (canonical == NULL)
        ai->ai_canonname = NULL;
    else {
        ai->ai_canonname = strdup(canonical);
        if (ai->ai_canonname == NULL) {
            freeaddrinfo(ai);
            return NULL;
        }
    }
    memset(ai->ai_addr, 0, sizeof(struct sockaddr_in));
    ai->ai_flags = 0;
    ai->ai_family = AF_INET;
    ai->ai_socktype = socktype;
    ai->ai_protocol = IPPROTO_IP;
    ai->ai_addrlen = sizeof(struct sockaddr_in);
    ((struct sockaddr_in *) ai->ai_addr)->sin_family = AF_INET;
    ((struct sockaddr_in *) ai->ai_addr)->sin_addr = addr;
    ((struct sockaddr_in *) ai->ai_addr)->sin_port = htons(port);
    sin_set_length((struct sockaddr_in *) ai->ai_addr);
    return ai;
}


/*
**  Look up a service.  Takes the service name (which may be numeric), the
**  hint flags, a pointer to the socket type (used to determine whether TCP or
**  UDP services are of interest and, if 0, is filled in with the result of
**  getservbyname if the service was not numeric), and a pointer to the
**  addrinfo struct to fill in.  Returns 0 on success or an EAI_* error on
**  failure.
*/
static int
gai_service(const char *servname, int flags, int *type, unsigned short *port)
{
    struct servent *servent;
    const char *protocol;
    long value;

    if (convert_service(servname, &value)) {
        if (value > (1L << 16) - 1)
            return EAI_SERVICE;
        *port = value;
    } else {
        if (flags & AI_NUMERICSERV)
            return EAI_NONAME;
        if (*type != 0)
            protocol = (*type == SOCK_DGRAM) ? "udp" : "tcp";
        else
            protocol = NULL;

        /* We really technically should be generating an addrinfo struct for
           each possible protocol unless type is set, but this works well
           enough for what I need this for. */
        servent = getservbyname(servname, protocol);
        if (servent == NULL)
            return EAI_NONAME;
        if (strcmp(servent->s_proto, "udp") == 0)
            *type = SOCK_DGRAM;
        else if (strcmp(servent->s_proto, "tcp") == 0)
            *type = SOCK_STREAM;
        else
            return EAI_SERVICE;
        *port = htons(servent->s_port);
    }
    return 0;
}


/*
**  Look up a host and fill in a linked list of addrinfo structs with the
**  results, one per IP address of the returned host.  Takes the name or IP
**  address of the host as a string, the lookup flags, the type of socket (to
**  fill into the addrinfo structs), the port (likewise), and a pointer to
**  where the head of the linked list should be put.  Returns 0 on success or
**  the appropriate EAI_* error.
*/
static int
gai_lookup(const char *nodename, int flags, int socktype, unsigned short port,
           struct addrinfo **res)
{
    struct addrinfo *ai, *first, *prev;
    struct in_addr addr;
    struct hostent *host;
    const char *canonical;
    int i;

    if (inet_aton(nodename, &addr)) {
        canonical = (flags & AI_CANONNAME) ? nodename : NULL;
        ai = gai_addrinfo_new(socktype, canonical, addr, port);
        if (ai == NULL)
            return EAI_MEMORY;
        *res = ai;
        return 0;
    } else {
        if (flags & AI_NUMERICHOST)
            return EAI_NONAME;
        host = gethostbyname(nodename);
        if (host == NULL)
            switch (h_errno) {
            case HOST_NOT_FOUND:
                return EAI_NONAME;
            case TRY_AGAIN:
            case NO_DATA:
                return EAI_AGAIN;
            case NO_RECOVERY:
                return EAI_FAIL;
            case NETDB_INTERNAL:
            default:
                return EAI_SYSTEM;
            }
        if (host->h_addr_list[0] == NULL)
            return EAI_FAIL;
        canonical = (flags & AI_CANONNAME)
            ? ((host->h_name != NULL) ? host->h_name : nodename)
            : NULL;
        first = NULL;
        prev = NULL;
        for (i = 0; host->h_addr_list[i] != NULL; i++) {
            if (host->h_length != sizeof(addr)) {
                freeaddrinfo(first);
                return EAI_FAIL;
            }
            memcpy(&addr, host->h_addr_list[i], sizeof(addr));
            ai = gai_addrinfo_new(socktype, canonical, addr, port);
            if (ai == NULL) {
                freeaddrinfo(first);
                return EAI_MEMORY;
            }
            if (first == NULL) {
                first = ai;
                prev = ai;
            } else {
                prev->ai_next = ai;
                prev = ai;
            }
        }
        *res = first;
        return 0;
    }
}


/*
**  The actual getaddrinfo implementation.
*/
int
getaddrinfo(const char *nodename, const char *servname,
            const struct addrinfo *hints, struct addrinfo **res)
{
    struct addrinfo *ai;
    struct in_addr addr;
    int flags, socktype, status;
    unsigned short port;

    /* Take the hints into account and check them for validity. */
    if (hints != NULL) {
        flags = hints->ai_flags;
        socktype = hints->ai_socktype;
        if ((flags & AI_INTERNAL_ALL) != flags)
            return EAI_BADFLAGS;
        if (hints->ai_family != AF_UNSPEC && hints->ai_family != AF_INET)
            return EAI_FAMILY;
        if (socktype != 0 && socktype != SOCK_STREAM && socktype != SOCK_DGRAM)
            return EAI_SOCKTYPE;

        /* EAI_SOCKTYPE isn't quite right, but there isn't anything better. */
        if (hints->ai_protocol != 0)
            if (hints->ai_protocol != IPPROTO_IP)
                return EAI_SOCKTYPE;
    } else {
        flags = 0;
        socktype = 0;
    }

    /* See what we're doing.  If nodename is null, either AI_PASSIVE is set or
       we're getting information for connecting to a service on the loopback
       address.  Otherwise, we're getting information for connecting to a
       remote system. */
    if (servname == NULL)
        port = 0;
    else {
        status = gai_service(servname, flags, &socktype, &port);
        if (status != 0)
            return status;
    }
    if (nodename != NULL)
        return gai_lookup(nodename, flags, socktype, port, res);
    else {
        if (servname == NULL)
            return EAI_NONAME;
        addr.s_addr = (flags & AI_PASSIVE) ? INADDR_ANY : INADDR_LOOPBACK;
        ai = gai_addrinfo_new(socktype, NULL, addr, port);
        if (ai == NULL)
            return EAI_MEMORY;
        *res = ai;
        return 0;
    }
}
