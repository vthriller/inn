/*  $Id$
**
**  Generate a message ID.
*/
#include "config.h"
#include <sys/types.h>

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#include "libinn.h"
#include "macros.h"

/* Scale time back a bit, for shorter Message-ID's. */
#define OFFSET	673416000L

char *
GenerateMessageID(char *domain)
{
    static char		buff[SMBUF];
    static int		count;
    char		*p;
    char		sec32[10];
    char		pid32[10];
    TIMEINFO		Now;

    if (GetTimeInfo(&Now) < 0)
	return NULL;
    Radix32(Now.time - OFFSET, sec32);
    Radix32(getpid(), pid32);
    if ((domain != NULL && innconf->domain == NULL) ||
	(domain != NULL && innconf->domain != NULL && !EQ(domain, innconf->domain))) {
	p = domain;
    } else {
	if ((p = GetFQDN(domain)) == NULL)
	    return NULL;
    }
    sprintf(buff, "<%s$%s$%d@%s>", sec32, pid32, ++count, p);
    return buff;
}
