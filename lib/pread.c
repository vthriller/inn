/*
** $Revision 1.0 $
*/

#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <stdio.h>
#include "config.h"
#include "clibrary.h"

#ifndef HAVE_PREAD

OFFSET_T pread(int fd, void *buf, OFFSET_T nbyte, OFFSET_T offset ) {
    
    if (lseek(fd, offset, SEEK_SET) < 0)
	return -1;

    return read(fd, buf, nbyte);
}

#endif /* HAVE_PREAD */
/*
** $Revision 1.0 $
*/

#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <stdio.h>
#include "config.h"
#include "clibrary.h"

#ifndef HAVE_PREAD

OFFSET_T pread(int fd, void *buf, OFFSET_T nbyte, OFFSET_T offset ) {
    
    if (lseek(fd, offset, SEEK_SET) < 0)
	return -1;

    return read(fd, buf, nbyte);
}

#endif /* HAVE_PREAD */
/*
** $Revision 1.0 $
*/

#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <stdio.h>
#include "config.h"
#include "clibrary.h"

#ifndef HAVE_PREAD

OFFSET_T pread(int fd, void *buf, OFFSET_T nbyte, OFFSET_T offset ) {
    
    if (lseek(fd, offset, SEEK_SET) < 0)
	return -1;

    return read(fd, buf, nbyte);
}

#endif /* HAVE_PREAD */
