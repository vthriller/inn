/*  $Id$
**
**  trashing articles method header
*/

#ifndef __TRASH_H__
#define __TRASH_H__

#include <configdata.h>
#include <interface.h>

BOOL trash_init(BOOL *selfexpire);
TOKEN trash_store(const ARTHANDLE article, const STORAGECLASS class);
ARTHANDLE *trash_retrieve(const TOKEN token, const RETRTYPE amount);
ARTHANDLE *trash_next(const ARTHANDLE *article, const RETRTYPE amount);
void trash_freearticle(ARTHANDLE *article);
BOOL trash_cancel(TOKEN token);
void trash_shutdown(void);

#endif
