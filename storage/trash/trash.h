/*  $Id$
**
**  trashing articles method header
*/

#ifndef __TRASH_H__
#define __TRASH_H__

#include <configdata.h>
#include <interface.h>

BOOL trash_init(SMATTRIBUTE *attr);
TOKEN trash_store(const ARTHANDLE article, const STORAGECLASS class);
ARTHANDLE *trash_retrieve(const TOKEN token, const RETRTYPE amount);
ARTHANDLE *trash_next(const ARTHANDLE *article, const RETRTYPE amount);
void trash_freearticle(ARTHANDLE *article);
BOOL trash_cancel(TOKEN token);
BOOL trash_ctl(PROBETYPE type, TOKEN *token, void *value);
BOOL trash_flushcacheddata(FLUSHTYPE type);
void trash_printfiles(FILE *file, TOKEN token, char **xref, int ngroups);
void trash_shutdown(void);

#endif
