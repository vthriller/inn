/*  $Id$
**
**  Storage Manager interface header
*/

#ifndef __INTERFACE_H__
#define __INTERFACE_H__

#include "config.h"
#include "storage.h"
#include <stdio.h>

typedef struct {
    bool	selfexpire;
    bool	expensivestat;
} SMATTRIBUTE;

typedef struct {
    char                *name;
    unsigned char       type;
    bool                (*init)(SMATTRIBUTE *attr);
    TOKEN               (*store)(const ARTHANDLE article, const STORAGECLASS storageclass);
    ARTHANDLE           *(*retrieve)(const TOKEN token, const RETRTYPE amount);
    ARTHANDLE           *(*next)(const ARTHANDLE *article, const RETRTYPE amount);
    void                (*freearticle)(ARTHANDLE *article);
    bool                (*cancel)(TOKEN token);
    bool                (*ctl)(PROBETYPE type, TOKEN *token, void *value);
    bool                (*flushcacheddata)(FLUSHTYPE type);
    void                (*printfiles)(FILE *, TOKEN, char **xref, int ngroups);
    void                (*shutdown)(void);
} STORAGE_METHOD;

typedef struct __S_SUB__ {
    int                 type;        /* Index into storage_methods of the one to use */
    int                 minsize;     /* Minimum size to send to this method */
    int                 maxsize;     /* Maximum size to send to this method */
    time_t		minexpire;   /* Minimum expire offset to send method */
    time_t		maxexpire;   /* Maximum expire offset to send method */
    int                 numpatterns; /* Number of patterns in patterns */
    int                 class;       /* Number of the storage class for this subscription */
    char                **patterns;  /* Array of patterns to check against
					the groups to determine if the article
					should go to this method */
    char		*options;    /* additional options specific to the
				        method */
    bool		exactmatch;  /* all newsgroups to which article belongs
				        should match the patterns */
    struct __S_SUB__   *next;
} STORAGE_SUB;

extern bool SMopenmode;
extern bool SMpreopen;
char *SMFindBody(char *article, int len);
STORAGE_SUB *SMGetConfig(STORAGETYPE type, STORAGE_SUB *sub);
STORAGE_SUB *SMgetsub(const ARTHANDLE article);
void SMseterror(int errorno, char *error);

#endif /* __INTERFACE_H__ */
