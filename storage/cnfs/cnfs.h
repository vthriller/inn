#ifndef __CNFS_H__
#define __CNFS_H__

extern __CNFS_Write_Allowed, __CNFS_Cancel_Allowed;

BOOL cnfs_init(void);
TOKEN cnfs_store(const ARTHANDLE article, const STORAGECLASS class);
ARTHANDLE *cnfs_retrieve(const TOKEN token, RETRTYPE amount);
ARTHANDLE *cnfs_next(const ARTHANDLE *article, RETRTYPE amount);
void cnfs_freearticle(ARTHANDLE *article);
BOOL cnfs_cancel(TOKEN token);
void cnfs_shutdown(void);

#endif
