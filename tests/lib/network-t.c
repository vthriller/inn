/* $Id$ */
/* network test suite. */

#include "config.h"
#include "clibrary.h"
#include "portable/socket.h"
#include <errno.h>

#include "inn/innconf.h"
#include "inn/messages.h"
#include "inn/network.h"
#include "libinn.h"
#include "libtest.h"

/* The server portion of the test.  Listens to a socket and accepts a
   connection, making sure what is printed on that connection matches what the
   client is supposed to print. */
static int
listener(int fd, int n)
{
    int client;
    FILE *out;
    char buffer[512];

    alarm(1);
    client = accept(fd, NULL, NULL);
    close(fd);
    if (client < 0) {
        syswarn("cannot accept connection from socket");
        ok_block(n, 2, false);
        return n + 2;
    }
    ok(n++, true);
    out = fdopen(client, "r");
    if (fgets(buffer, sizeof(buffer), out) == NULL) {
        syswarn("cannot read from socket");
        ok(n++, false);
        return n;
    }
    ok_string(n++, "socket test\r\n", buffer);
    fclose(out);
    return n;
}

/* Connect to the given host on port 11119 and send a constant string to a
   socket, used to do the client side of the testing.  Takes the source
   address as well to pass into network_connect_host. */
static void
client(const char *host, const char *source)
{
    int fd;
    FILE *out;

    fd = network_connect_host(host, 11119, source);
    if (fd < 0)
        _exit(1);
    out = fdopen(fd, "w");
    if (out == NULL)
        _exit(1);
    fputs("socket test\r\n", out);
    fclose(out);
    _exit(0);
}

/* Bring up a server on port 11119 on the loopback address and test connecting
   to it via IPv4.  Takes an optional source address to use for client
   connections. */
static int
test_ipv4(int n, const char *source)
{
    int fd;
    pid_t child;

    fd = network_bind_ipv4("127.0.0.1", 11119);
    if (fd < 0)
        sysdie("cannot create or bind socket");
    if (listen(fd, 1) < 0) {
        syswarn("cannot listen to socket");
        ok(n++, false);
        ok(n++, false);
        ok(n++, false);
    } else {
        ok(n++, true);
        child = fork();
        if (child < 0)
            sysdie("cannot fork");
        else if (child == 0)
            client("127.0.0.1", source);
        else
            n = listener(fd, n);
    }
    return n;
}

/* Bring up a server on port 11119 on the loopback address and test connecting
   to it via IPv6.  Takes an optional source address to use for client
   connections. */
#ifdef HAVE_INET6
static int
test_ipv6(int n, const char *source)
{
    int fd;
    pid_t child;

    fd = network_bind_ipv6("::1", 11119);
    if (fd < 0) {
        if (errno == EAFNOSUPPORT || errno == EPROTONOSUPPORT) {
            skip_block(n, 3, "IPv6 not supported");
            return n + 3;
        } else
            sysdie("cannot create socket");
    }
    if (listen(fd, 1) < 0) {
        syswarn("cannot listen to socket");
        ok_block(n, 3, false);
        n += 3;
    } else {
        ok(n++, true);
        child = fork();
        if (child < 0)
            sysdie("cannot fork");
        else if (child == 0)
            client("::1", source);
        else
            n = listener(fd, n);
    }
    return n;
}
#else /* !HAVE_INET6 */
static int
test_ipv6(int n, const char *source UNUSED)
{
    skip_block(n, 3, "IPv6 not supported");
    return n + 3;
}
#endif /* !HAVE_INET6 */

/* Bring up a server on port 11119 on all addresses and try connecting to it
   via all of the available protocols.  Takes an optional source address to
   use for client connections. */
static int
test_all(int n, const char *source_ipv4, const char *source_ipv6)
{
    int *fds, count, fd, i;
    pid_t child;
    struct sockaddr_storage saddr;
    size_t size = sizeof(saddr);

    network_bind_all(11119, &fds, &count);
    if (count == 0)
        sysdie("cannot create or bind socket");
    if (count > 2) {
        warn("got more than two sockets, using just the first two");
        count = 2;
    }
    for (i = 0; i < count; i++) {
        fd = fds[i];
        if (listen(fd, 1) < 0) {
            syswarn("cannot listen to socket %d", fd);
            ok_block(n, 3, false);
            n += 3;
        } else {
            ok(n++, true);
            child = fork();
            if (child < 0)
                sysdie("cannot fork");
            else if (child == 0) {
                if (getsockname(fd, (struct sockaddr *) &saddr, &size) < 0)
                    sysdie("cannot getsockname");
                if (saddr.ss_family == AF_INET)
                    client("127.0.0.1", source_ipv4);
                else if (saddr.ss_family == AF_INET6)
                    client("::1", source_ipv6);
                else {
                    warn("unknown socket family %d", saddr.ss_family);
                    skip_block(n, 2, "unknown socket family");
                    n += 2;
                }
                size = sizeof(saddr);
            } else
                n = listener(fd, n);
        }
    }
    if (count == 1) {
        skip_block(n, 3, "only one listening socket");
        n += 3;
    }
    return n;
}

int
main(void)
{
    int n;

    test_init(48);

    n = test_ipv4(1, NULL);        /* Tests  1-3.  */
    n = test_ipv6(n, NULL);        /* Tests  4-6.  */
    n = test_all(n, NULL, NULL);   /* Tests  7-12. */

    /* This won't make a difference for loopback connections. */
    n = test_ipv4(n, "127.0.0.1"); /* Tests 13-15. */
    n = test_ipv6(n, "::1");       /* Tests 16-18. */
    n = test_all(n, "127.0.0.1", "::1");  /* Tests 19-24. */

    /* We need an initialized innconf struct, but it doesn't need to contain
       anything interesting. */
    innconf = xcalloc(1, sizeof(struct innconf));

    /* This should be equivalent to the previous tests. */
    innconf->sourceaddress = xstrdup("all");
    innconf->sourceaddress6 = xstrdup("all");
    n = test_ipv4(n, NULL);        /* Tests 25-27. */
    n = test_ipv6(n, NULL);        /* Tests 28-30. */
    n = test_all(n, NULL, NULL);   /* Tests 31-36. */

    /* This won't make a difference for loopback connections. */
    free(innconf->sourceaddress);
    free(innconf->sourceaddress6);
    innconf->sourceaddress = xstrdup("127.0.0.1");
    innconf->sourceaddress6 = xstrdup("::1");
    n = test_ipv4(n, NULL);        /* Tests 37-39. */
    n = test_ipv6(n, NULL);        /* Tests 40-42. */
    n = test_all(n, NULL, NULL);   /* Tests 43-48. */

    return 0;
}