/*  $Revision$
**
*/
#include <stdio.h>
#include <sys/types.h>
#include "configdata.h"
#include "clibrary.h"
#include <errno.h>
#include "libinn.h"
#include "macros.h"


/*
**  Reallocate some memory or call the memory failure handler.
*/
ALIGNPTR xrealloc(char *p, unsigned int i)
{
    POINTER		new;

    while ((new = realloc((POINTER)p, i)) == NULL)
	(*xmemfailure)("remalloc", i);
    return CAST(ALIGNPTR, new);
}
