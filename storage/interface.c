/*  $Id$
**
**  Storage Manager interface
*/

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <macros.h>
#include <configdata.h>
#include <clibrary.h>
#include <libinn.h>
#include <logging.h>
#include <paths.h>
#include <methods.h>
#include <interface.h>
#include <errno.h>

typedef enum {INIT_NO, INIT_DONE, INIT_FAIL} INITTYPE;
typedef struct {
    INITTYPE		initialized;
    BOOL		configured;
    BOOL		selfexpire;
} METHOD_DATA;

METHOD_DATA method_data[NUM_STORAGE_METHODS];

static STORAGE_SUB      *subscriptions = NULL;
static unsigned int     typetoindex[256];
int                     SMerrno;
char                    *SMerrorstr = NULL;
static BOOL             ErrorAlloc = FALSE;
static BOOL             Initialized = FALSE;

/*
** Checks to see if the token is valid
*/
BOOL IsToken(const char *text) {
    const char          *p;
    
    if (!text)
	return FALSE;
    
    if (strlen(text) != (sizeof(TOKEN) * 2) + 2)
	return FALSE;
    
    if (text[0] != '@')
	return FALSE;

    if (text[(sizeof(TOKEN) * 2) + 1] != '@')
	return FALSE;

    for (p = text + 1; *p != '@'; p++)
	if (!isxdigit(*p))
	    return FALSE;
    
    return TRUE;
}

/*
** Converts a token to a textual representation for error messages
** and the like.
*/
char *TokenToText(const TOKEN token) {
    static char         hex[] = "0123456789ABCDEF";
    static char         result[(sizeof(TOKEN) * 2) + 3];
    char                *p;
    char                *q;
    char                i;

    result[0] = '@';
    for (q = result + 1, p = (char *)&token, i = 0; i < sizeof(TOKEN); i++, p++) {
	*q++ = hex[(*p & 0xF0) >> 4];
	*q++ = hex[*p & 0x0F];
    }
    *q++ = '@';
    *q++ = '\0';
    return result;
    
}

/*
** Converts a hex digit and converts it to a int
*/
STATIC int hextodec(const char c) {
    return isdigit(c) ? (c - '0') : ((c - 'A') + 10);
}

/*
** Converts a textual representation of a token back to a native
** representation
*/
TOKEN TextToToken(const char *text) {
    const char          *p;
    char                *q;
    int                 i;
    TOKEN               token;

    if (text[0] == '@')
	p = &text[1];
    else
	p = text;

    for (q = (char *)&token, i = 0; i != sizeof(TOKEN); i++) {
	q[i] = (hextodec(*p) << 4) + hextodec(*(p + 1));
	p += 2;
    }
    return token;
}

/*
**  Searches through the given string and find the begining of the
**  message body and returns that if it finds it.  If not, it returns
**  NULL.
*/
char *SMFindBody(char *article, int len) {
    char                *p;

    for (p = article; p < (article + len - 4); p++) {
	if (!memcmp(p, "\r\n\r\n", 4))
	    return p+4;
    }
    return NULL;
}

STORAGE_SUB *SMGetConfig(STORAGETYPE type, STORAGE_SUB *sub) {
    if (sub == (STORAGE_SUB *)NULL)
	sub = subscriptions;
    else
	sub = sub->next;
    for (;sub != NULL; sub = sub->next) {
	if (sub->type == type) {
	    return sub;
	}
    }
    return (STORAGE_SUB *)NULL;
}

/* Open the config file and parse it, generating the policy data */
static BOOL SMreadconfig(void) {
    char                path[sizeof(_PATH_STORAGECTL) + 16];
    FILE                *f;
    char                line[1024];
    int                 i;
    char                *p;
    char                *q;
    int                 linenum = 0;
    char                *method;
    char                *patterns;
    int                 minsize;
    int                 maxsize;
    int                 class;
    STORAGE_SUB         *sub = NULL;
    STORAGE_SUB         *prev = NULL;
    char		*options;

    for (i = 0; i < NUM_STORAGE_METHODS; i++) {
	method_data[i].initialized = INIT_NO;
	method_data[i].configured = FALSE;
    }
    sprintf(path, "%s", _PATH_STORAGECTL);
    if ((f = fopen(path, "r")) == NULL) {
	SMseterror(SMERR_UNDEFINED, NULL);
	syslog(L_ERROR, "SM Could not open %s: %m", path);
	return FALSE;
    }

    
    while (fgets(line, 1024, f) != NULL) {
	linenum++;
	if ((p = strchr(line, '#')) != NULL)
	    *p = '\0';
	
	for (p = q = line; *p != '\0'; p++)
	    if (!isspace(*p))
		*q++ = *p;

	*q = '\0';
	
	if (!line[0])
	    continue;
	
	if ((p = strchr(line, ':')) == NULL) {
	    SMseterror(SMERR_CONFIG, "Could not find end of the first field");
	    syslog(L_ERROR, "SM could not find end of first field, line %d", linenum);
	    return FALSE;
	}
	method = line;
	*p = '\0';
	patterns = ++p;
	class = minsize = maxsize = 0;
	options = (char *)NULL;
	if ((p = strchr(p, ':')) != NULL) {
	    *p = '\0';
	    p++;
	}
	if (p && *p) {
	    q = p;
	    if ((p = strchr(p, ':')) != NULL) {
		*p = '\0';
		class = atoi(q);
		p++;
		if (*p) {
		    q = p;
		    if ((p = strchr(p, ':')) != NULL) {
			*p = '\0';
		        minsize = atoi(q);
		        p++;
		        if (*p) {
			    q = p;
			    if ((p = strchr(++p, ':')) != NULL) {
				*p = '\0';
				maxsize = atoi(q);
				p++;
				if (*p) {
				    options = COPY(p);
				}
			    } else
				maxsize = atoi(q);
		        }
		    } else
			minsize = atoi(q);
		}
	    } else
		class = atoi(q);
	}
	sub = NEW(STORAGE_SUB, 1);
	sub->type = TOKEN_EMPTY;
	for (i = 0; i < NUM_STORAGE_METHODS; i++) {
	    if (!strcasecmp(method, storage_methods[i].name)) {
		sub->type = storage_methods[i].type;
		method_data[i].configured = TRUE;
		break;
	    }
	}
	if (sub->type == TOKEN_EMPTY) {
	    SMseterror(SMERR_CONFIG, "Invalid storage method name");
	    syslog(L_ERROR, "SM no configured storage methods are named '%s'", method);
	    DISPOSE(options);
	    DISPOSE(sub);
	    return FALSE;
	}
	sub->minsize = minsize;
	sub->maxsize = maxsize;
	sub->class = class;
	sub->options = options;
	
	/* Count the number of patterns and allocate space*/
	for (i = 1, p = patterns; *p && (p = strchr(p+1, ',')); i++);

	sub->numpatterns = i;
	sub->patterns = NEW(char *, i);
	if (!prev)
	    subscriptions = sub;

	/* Store the patterns. */
	for (i = 0, p = strtok(patterns, ","); p != NULL; i++, p = strtok(NULL, ","))
	    sub->patterns[i] = COPY(p);

	if (prev)
	    prev->next = sub;
	prev = sub;
	sub->next = NULL;
    }
    
    fclose(f);

    return TRUE;
}

/*
** Calls the setup function for all of the configured methods and returns
** TRUE if they all initialize ok, FALSE if they don't
*/
BOOL SMinit(void) {
    int                 i;
    BOOL		allok = TRUE;
    BOOL		selfexpire;

    if (Initialized)
	return TRUE;
    
    Initialized = TRUE;
    
    if (!SMreadconfig()) {
	Initialized = FALSE;
	return FALSE;
    }

    for (i = 0; i < NUM_STORAGE_METHODS; i++) {
	if (method_data[i].configured) {
	    if (method_data[i].configured && storage_methods[i].init(&selfexpire)) {
		method_data[i].initialized = INIT_DONE;
		method_data[i].selfexpire = selfexpire;
	    } else {
		method_data[i].initialized = INIT_FAIL;
		method_data[i].selfexpire = FALSE;
		syslog(L_ERROR, "SM storage method '%s' failed initialization", storage_methods[i].name);
		allok = FALSE;
	    }
	}
	typetoindex[storage_methods[i].type] = i;
    }
    if (!allok) {
	SMshutdown();
	Initialized = FALSE;
	SMseterror(SMERR_UNDEFINED, "one or more storage methods failed initialization");
	syslog(L_ERROR, "SM one or more storage methods failed initialization");
	return FALSE;
    }
    if (atexit(SMshutdown) < 0) {
	SMshutdown();
	Initialized = FALSE;
	SMseterror(SMERR_UNDEFINED, NULL);
	return FALSE;
    }
    return TRUE;
}

static BOOL InitMethod(STORAGETYPE method) {
    BOOL		selfexpire;

    if (!Initialized)
	if (!SMreadconfig()) {
	    Initialized = FALSE;
	    return FALSE;
	}
    Initialized = TRUE;
    
    if (method_data[method].initialized == INIT_DONE)
	return TRUE;

    if (method_data[method].initialized == INIT_FAIL)
	return FALSE;

    if (!method_data[typetoindex[method]].configured) {
	method_data[method].initialized = INIT_FAIL;
	SMseterror(SMERR_UNDEFINED, "storage method is not configured.");
	return FALSE;
    }
    if (!storage_methods[typetoindex[method]].init(&selfexpire)) {
	method_data[method].initialized = INIT_FAIL;
	method_data[method].selfexpire = FALSE;
	SMseterror(SMERR_UNDEFINED, "Could not initialize storage method late.");
	return FALSE;
    }
    method_data[method].initialized = INIT_DONE;
    method_data[method].selfexpire = selfexpire;
    return TRUE;
}

static BOOL MatchGroups(const char *g, int num, char **patterns) {
    char                *group;
    char                *groups;
    const char          *p;
    int                 i;
    BOOL                wanted = FALSE;

    /* Find the end of the line */
    for (p = g+1; (*p != '\n') && (*(p - 1) != '\r'); p++);

    groups = NEW(char, p - g);
    memcpy(groups, g, p - g - 1);
    groups[p - g - 1] = '\0';

    for (group = strtok(groups, ","); group != NULL; group = strtok(NULL, ",")) {
	for (i = 0; i < num; i++) {
	    switch (patterns[i][0]) {
	    case '!':
		if (!wanted && wildmat(group, &patterns[i][1]))
		    break;
	    case '@':
		if (wildmat(group, &patterns[i][1])) {
		    DISPOSE(groups);
		    return FALSE;
		}
	    default:
		if (wildmat(group, patterns[i]))
		    wanted = TRUE;
	    }
	}
    }

    DISPOSE(groups);
    return wanted;
}

TOKEN SMstore(const ARTHANDLE article) {
    STORAGE_SUB         *sub;
    TOKEN               result;
    char                *groups;

    result.type = TOKEN_EMPTY;
    if (!article.data || !article.len) {
	SMseterror(SMERR_BADHANDLE, NULL);
	return result;
    }

    if ((groups = (char *)HeaderFindMem(article.data, article.len, "Newsgroups", 10)) == NULL) {
	SMseterror(SMERR_UNDEFINED, "Could not find Newsgroups header");
	return result;
    }

    for (sub = subscriptions; sub != NULL; sub = sub->next) {
	if (!(method_data[typetoindex[sub->type]].initialized == INIT_FAIL) &&
	    (article.len >= sub->minsize) &&
	    (!sub->maxsize || (article.len <= sub->maxsize)) &&
	    MatchGroups(groups, sub->numpatterns, sub->patterns)) {
	    if (InitMethod(typetoindex[sub->type]))
		return storage_methods[typetoindex[sub->type]].store(article, sub->class);
	}
    }

    return result;
}

ARTHANDLE *SMretrieve(const TOKEN token, const RETRTYPE amount) {
    ARTHANDLE           *art;

    if (method_data[typetoindex[token.type]].initialized == INIT_FAIL) {
	syslog(L_ERROR, "SM method initialization was failed");
	SMseterror(SMERR_BADTOKEN, NULL);
	return NULL;
    }
    if (method_data[typetoindex[token.type]].initialized == INIT_NO && !InitMethod(typetoindex[token.type])) {
	syslog(L_ERROR, "SM could not find token type or method was not initialized");
	SMseterror(SMERR_BADTOKEN, NULL);
	return NULL;
    }
    art = storage_methods[typetoindex[token.type]].retrieve(token, amount);
    if (art)
	art->nextmethod = 0;
    return art;

}

ARTHANDLE *SMnext(const ARTHANDLE *article, const RETRTYPE amount) {
    unsigned char       i;
    int                 start;
    ARTHANDLE           *newart;

    if (article == NULL)
	start = 0;
    else
	start= article->nextmethod;

    if (method_data[start].initialized == INIT_FAIL) {
	SMseterror(SMERR_BADTOKEN, NULL);
	return NULL;
    }
    if (method_data[start].initialized == INIT_NO && method_data[i].configured
      && !InitMethod(start)) {
	SMseterror(SMERR_UNINIT, NULL);
	return NULL;
    }

    for (i = start, newart = NULL; i < NUM_STORAGE_METHODS; i++) {
	if (method_data[i].configured && (newart = storage_methods[i].next(article, amount)) != (ARTHANDLE *)NULL) {
	    newart->nextmethod = i;
	    break;
	}
    }

    return newart;
}

void SMfreearticle(ARTHANDLE *article) {
    if (method_data[typetoindex[article->type]].initialized == INIT_FAIL) {
	syslog(L_ERROR, "SM can't free article with initialization failed method");
	return;
    }
    if (method_data[typetoindex[article->type]].initialized == INIT_NO && !InitMethod(typetoindex[article->type])) {
	syslog(L_ERROR, "SM can't free article with uninitialized method");
	return;
    }
    storage_methods[typetoindex[article->type]].freearticle(article);
}

BOOL SMcancel(TOKEN token) {
    if (method_data[typetoindex[token.type]].initialized == INIT_FAIL) {
	SMseterror(SMERR_UNINIT, NULL);
	syslog(L_ERROR, "SM can't cancel article with initialization failed method");
	return;
    }
    if (method_data[typetoindex[token.type]].initialized == INIT_NO && !InitMethod(typetoindex[token.type])) {
	SMseterror(SMERR_UNINIT, NULL);
	syslog(L_ERROR, "SM can't cancel article with uninitialized method");
	return FALSE;
    }
    return storage_methods[typetoindex[token.type]].cancel(token);
}

BOOL SMprobe(PROBETYPE type, TOKEN *token) {
    switch (type) {
    case SELFEXPIRE:
	return (method_data[typetoindex[token->type]].selfexpire);
    default:
	return FALSE;
    }
}

void SMshutdown(void) {
    int                 i;
    STORAGE_SUB         *old;

    if (!Initialized)
	return;

    for (i = 0; i < NUM_STORAGE_METHODS; i++)
	if (method_data[i].initialized == INIT_DONE) {
	    storage_methods[i].shutdown();
	    method_data[i].initialized = INIT_NO;
	    method_data[i].configured = FALSE;
	}
    while (subscriptions) {
	old = subscriptions;
	subscriptions = subscriptions->next;
	DISPOSE(old->patterns);
	DISPOSE(old->options);
	DISPOSE(old);
    }
    Initialized = FALSE;
}

void SMseterror(int errornum, char *error) {
    if (ErrorAlloc)
	DISPOSE(SMerrorstr);

    ErrorAlloc = FALSE;
    
    if ((errornum == SMERR_UNDEFINED) && (errno == ENOENT))
	errornum = SMERR_NOENT;
	    
    SMerrno = errornum;

    if (error == NULL) {
	switch (SMerrno) {
	case SMERR_UNDEFINED:
	    SMerrorstr = COPY(strerror(errno));
	    ErrorAlloc = TRUE;
	    break;
	case SMERR_INTERNAL:
	    SMerrorstr = "Internal error";
	    break;
	case SMERR_NOENT:
	    SMerrorstr = "Token not found";
	    break;
	case SMERR_TOKENSHORT:
	    SMerrorstr = "Configured token size too small";
	    break;
	case SMERR_NOBODY:
	    SMerrorstr = "No article body found";
	    break;
	case SMERR_UNINIT:
	    SMerrorstr = "Storage manager is not initialized";
	    break;
	case SMERR_CONFIG:
	    SMerrorstr = "Error reading config file";
	    break;
	case SMERR_BADHANDLE:
	    SMerrorstr = "Bad article handle";
	    break;
	case SMERR_BADTOKEN:
	    SMerrorstr = "Bad token";
	    break;
	default:
	    SMerrorstr = "Undefined error";
	}
    } else {
	SMerrorstr = COPY(error);
	ErrorAlloc = TRUE;
    }
}

