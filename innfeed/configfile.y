%{
/* -*- text -*-
 *
 * Author:      James Brister <brister@vix.com> -- berkeley-unix --
 * Start Date:  Fri, 17 Jan 1997 16:09:10 +0100
 * Project:     INN (innfeed)
 * File:        config.y
 * RCSId:       $Id$
 * Description: 
 * 
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <syslog.h>
  
#include "config.h"
#include "configfile.h"
#include "msgs.h"
#include "misc.h"

#define UNKNOWN_SCOPE_TYPE "line %d: unknown scope type: %s"
#define SYNTAX_ERROR "line %d: syntax error"

extern int lineCount ;
scope *topScope = NULL ;
static scope *currScope = NULL ;
char *errbuff = NULL ;

static void appendName (scope *s, char *p) ;
static char *valueScopedName (value *v) ;
static void freeValue (value *v) ;
static char *checkName (scope *s, const char *name) ;
static void addValue (scope *s, value *v) ;
static char *addScope (scope *s, const char *name, scope *val) ;
static void printScope (FILE *fp, scope *s, int indent) ;
static void printValue (FILE *fp, value *v, int indent) ;
static scope *newScope (const char *type) ;
#if 0
static int strNCaseCmp (const char *a, const char *b, size_t len) ;
#endif 



#if 0
int isString (scope *s, const char *name, int inherit)
{
  value *v = findValue (s,name,inherit) ;

  return (v != NULL && v->type == stringval) ;
}
#endif 

int getBool (scope *s, const char *name, int *rval, int inherit)
{
  value *v = findValue (s,name,inherit) ;

  if (v == NULL)
    return 0 ;
  else if (v->type != boolval)
    return 0 ;

  *rval = v->v.bool_val ;
  return 1 ;
}


int getString (scope *s, const char *name, char **rval, int inherit)
{
  value *v = findValue (s,name,inherit) ;

  if (v == NULL)
    return 0 ;
  else if (v->type != stringval)
    return 0 ;

  *rval = strdup (v->v.charp_val) ;
  return 1 ;
}


int getReal (scope *s, const char *name, double *rval, int inherit)
{
  value *v = findValue (s,name,inherit) ;

  if (v == NULL)
    return 0 ;
  else if (v->type != realval)
    return 0 ;

  *rval = v->v.real_val ;
  return 1 ;
}

int getInteger (scope *s, const char *name, long *rval, int inherit)
{
  value *v = findValue (s,name,inherit) ;

  if (v == NULL)
    return 0 ;
  else if (v->type != intval)
    return 0 ;

  *rval = v->v.int_val ;
  return 1 ;
}

void freeScopeTree (scope *s)
{
  int i ;

  if (s == NULL)
    return ;

  if (s->parent == NULL && s->me != NULL) 
    {                           /* top level scope */
      free (s->me->name) ;
      free (s->me) ;
    }
  
  
  for (i = 0 ; i < s->value_idx ; i++)
    if (s->values[i] != NULL)
	freeValue (s->values [i]) ;

  free (s->values) ;
  free (s->scope_type) ;

  s->parent = NULL ;
  s->values = NULL ;

  free (s) ;
}


char *addInteger (scope *s, const char *name, long val) 
{
  value *v ;
  char *error ;
  
  if ((error = checkName (currScope,name)) != NULL)
    return error ;

  v = (value *) calloc (1,sizeof (value)) ;
  v->name = strdup (name) ;
  v->type = intval ;
  v->v.int_val = val ;
  
  addValue (s,v) ;

  return NULL ;
}

char *addChar (scope *s, const char *name, char val) 
{
  value *v ;
  char *error ;
  
  if ((error = checkName (currScope,name)) != NULL)
    return error ;

  v = (value *) calloc (1,sizeof (value)) ;
  v->name = strdup (name) ;
  v->type = charval ;
  v->v.char_val = val ;
  
  addValue (s,v) ;

  return NULL ;
}

char *addBoolean (scope *s, const char *name, int val) 
{
  value *v ;
  char *error ;
  
  if ((error = checkName (currScope,name)) != NULL)
    return error ;

  v = (value *) calloc (1,sizeof (value)) ;
  v->name = strdup (name) ;
  v->type = boolval ;
  v->v.bool_val = val ;
  
  addValue (s,v) ;

  return NULL ;
}

char *addReal (scope *s, const char *name, double val)
{
  value *v ;
  char *error ;

  if ((error = checkName (currScope,name)) != NULL)
    return error ;

  v = (value *) calloc (1,sizeof (value)) ;
  v->name = strdup (name) ;
  v->type = realval ;
  v->v.real_val = val ;
  
  addValue (s,v) ;

  return NULL ;
}

char *addString (scope *s, const char *name, const char *val)
{
  value *v ;
  char *error ;

  if ((error = checkName (currScope,name)) != NULL)
    return error ;

  v = (value *) calloc (1,sizeof (value)) ;
  v->name = strdup (name) ;
  v->type = stringval ;
  v->v.charp_val = strdup (val) ;
  
  addValue (s,v) ;

  return NULL ;
}

value *findValue (scope *s, const char *name, int inherit) 
{
  const char *p ;
  
  if (name == NULL || *name == '\0')
    return NULL ;

  if (*name == ':')
    return findValue (topScope,name + 1,0) ;
  else if (s == NULL)
    return findValue (topScope,name,0) ;
  else 
    {
      int i ;
      
      if ((p = strchr (name,':')) == NULL)
        p = name + strlen (name) ;

      for (i = 0 ; i < s->value_idx ; i++)
        {
          if (strncmp (s->values[i]->name,name,p - name) == 0)
            {
              if (*p == '\0')     /* last segment of name */
                return s->values[i] ;
              else if (s->values[i]->type != scopeval)
                errbuff = strdup ("Component not a scope") ;
              else
                return findValue (s->values[i]->v.scope_val,p + 1,0) ;
            }
        }

      /* not in this scope. Go up if inheriting values and only if no ':'
         in name */
      if (inherit && *p == '\0')
        return findValue (s->parent,name,inherit) ;
    }

  return NULL ;
}

/* find the scope that name belongs to. If mustExist is true then the name
   must be a fully scoped name of a value. relative scopes start at s.  */
scope *findScope (scope *s, const char *name, int mustExist)
{
  scope *p = NULL ;
  char *q ;
  int i ;

  
  if ((q = strchr (name,':')) == NULL)
    {
      if (!mustExist)
        p = s ;
      else
        for (i = 0 ; p == NULL && i < s->value_idx ; i++)
          if (strcmp (s->values[i]->name,name) == 0)
            p = s ;
      
      return p ;
    }
  else if (*name == ':')
    {
      while (s->parent != NULL)
        s = s->parent ;

      return findScope (s,name + 1,mustExist) ;
    }
  else
    {
      for (i = 0 ; i < s->value_idx ; i++)
        if (strncmp (s->values[i]->name,name,q - name) == 0)
          if (s->values[i]->type == scopeval)
            return findScope (s->values[i]->v.scope_val,q + 1,mustExist) ;
    }

  return NULL ;
}

/****************************************************************************/
/*                                                                          */
/****************************************************************************/


static void appendName (scope *s, char *p) 
{
  if (s == NULL)
    return ;
  else
    {
      appendName (s->parent,p) ;
      strcat (p,s->me->name) ;
      strcat (p,":") ;
    }
}

static char *valueScopedName (value *v)
{
  scope *p = v->myscope ;
  int len = strlen (v->name) ;
  char *q ;
  
  while (p != NULL)
    {
      len += strlen (p->me->name) + 1 ;
      p = p->parent ;
    }

  q = malloc (len + 1) ;
  q [0] = '\0' ;
  appendName (v->myscope,q) ;
  strcat (q,v->name) ;

  return q ;
}

static void freeValue (value *v)
{
  free (v->name) ;
  switch (v->type)
    {
      case scopeval:
        freeScopeTree (v->v.scope_val) ;
        break ;

      case stringval:
        free (v->v.charp_val) ;
        break ;

      default:
        break ;
    }
  free (v) ;
}

static char *checkName (scope *s, const char *name)
{
  int i ;	
  char *error = NULL ;

  if (s == NULL)
    return NULL ;
  
  for (i = 0 ; i < s->value_idx ; i++)
    {
      char *n = NULL ;
      
      if (strcmp (name,s->values [i]->name) == 0) {

#define FMT "Two definitions of %s"

        n = valueScopedName (s->values[i]) ;
        error = malloc (strlen (FMT) + strlen (n) + 2) ;
        sprintf (error,FMT,n) ;
        free (n) ;
        return error ;
      }
    }
  
  return error ;
}


static void addValue (scope *s, value *v) 
{
  v->myscope = s ;
  
  if (s == NULL)
    return ;
      
  if (s->value_count == s->value_idx)
    {
      if (s->values == 0)
        {
          s->values = (value **) calloc (10,sizeof (value *)) ;
          s->value_count = 10 ;
        }
      else
        {
          s->value_count += 10 ;
          s->values = (value **) realloc (s->values,
                                          sizeof (value *) * s->value_count);
        }
    }
  
  s->values [s->value_idx++] = v ;
}



static char *addScope (scope *s, const char *name, scope *val)
{
  value *v ;
  char *error ;

  if ((error = checkName (s,name)) != NULL)
    return error ;

  v = (value *) calloc (1,sizeof (value)) ;
  v->name = strdup (name) ;
  v->type = scopeval ;
  v->v.scope_val = val ;
  val->me = v ;
  val->parent = s ;

  addValue (s,v) ;

  currScope = val ;

  return NULL ;
}


static void printScope (FILE *fp, scope *s, int indent)
{
  int i ;
  for (i = 0 ; i < s->value_idx ; i++)
    printValue (fp,s->values [i],indent + 5) ;
}

static void printValue (FILE *fp, value *v, int indent) 
{
  int i ;
  
  for (i = 0 ; i < indent ; i++)
    fputc (' ',fp) ;
  
  switch (v->type) 
    {
      case intval:
        fprintf (fp,"%s : %ld # INTEGER\n",v->name,v->v.int_val) ;
        break ;
        
      case stringval:
        fprintf (fp,"%s : \"",v->name) ;
        {
          char *p = v->v.charp_val ;
          while (*p) 
            {
              if (*p == '"' || *p == '\\')
                fputc ('\\',fp) ;
              fputc (*p,fp) ;
              p++ ;
            }
        }
        fprintf (fp,"\" # STRING\n") ;
        break ;

      case charval:
        fprintf (fp,"%s : %c",v->name,047) ;
        switch (v->v.char_val)
          {
            case '\\':
              fprintf (fp,"\\\\") ;
              break ;

            default:
              if (isprint (v->v.char_val))
                fprintf (fp,"%c",v->v.char_val) ;
              else
                fprintf (fp,"\\%03o",v->v.char_val) ;
          }
        fprintf (fp,"%c # CHARACTER\n",047) ;
        break ;
        
      case realval:
        fprintf (fp,"%s : %f # REAL\n",v->name,v->v.real_val) ;
        break ;

      case boolval:
        fprintf (fp,"%s : %s # BOOLEAN\n",
                 v->name,(v->v.bool_val ? "true" : "false")) ;
        break ;
        
      case scopeval:
        fprintf (fp,"%s %s { # SCOPE\n",v->v.scope_val->scope_type,v->name) ;
        printScope (fp,v->v.scope_val,indent + 5) ;
        for (i = 0 ; i < indent ; i++)
          fputc (' ',fp) ;
        fprintf (fp,"}\n") ;
        break ;

      default:
        fprintf (fp,"UNKNOWN value type: %d\n",v->type) ;
        exit (1) ;
    }
}

  

static scope *newScope (const char *type)
{
  scope *t ;
  int i ;
  
  t = (scope *) calloc (1,sizeof (scope)) ;
  t->parent = NULL ;
  t->scope_type = strdup (type) ;

  for (i = 0 ; t->scope_type[i] != '\0' ; i++)
    t->scope_type[i] = tolower (t->scope_type[i]) ;

  return t ;
}



#if 0
static int strNCaseCmp (const char *a, const char *b, size_t len)
{
  while (a && b && *a && *b && (tolower (*a) == tolower (*b)) && len > 0)
    a++, b++, len-- ;

  if (a == NULL && b == NULL)
    return 0 ;
  else if (a == NULL)
    return 1 ;
  else if (b == NULL)
    return -1 ;
  else if (*a == '\0' && *b == '\0')
    return 0 ;
  else if (*a == '\0')
    return 1 ;
  else if (*b == '\0')
    return -1 ;
  else if (*a < *b)
    return 1 ;
  else if (*a > *b)
    return -1 ;
  else
    return 0 ;

  abort () ;
}
#endif

#define BAD_KEY "line %d: illegal key name: %s"
#define NON_ALPHA "line %d: keys must start with a letter: %s"

static char *keyOk (const char *key) 
{
  const char *p = key ;
  char *rval ;

  if (key == NULL)
    return strdup ("NULL key") ;
  else if (*key == '\0')
    return strdup ("EMPTY KEY") ;
  
  if (!isalpha(*p))
    {
      rval = malloc (strlen (NON_ALPHA) + strlen (key) + 15) ;
      sprintf (rval,NON_ALPHA,lineCount, key) ;
      return rval ;
    }

  p++ ;
  while (*p)
    {
      if (!(isalnum (*p) || *p == '_' || *p == '-'))
        {
          rval = malloc (strlen (BAD_KEY) + strlen (key) + 15) ;
          sprintf (rval,BAD_KEY,lineCount,key) ;
          return rval ;
        }
      p++ ;
    }

  return NULL ;
}

static PFIVP *funcs = NULL ;
static void **args = NULL ;
static int funcCount ;
static int funcIdx ;

void configAddLoadCallback (PFIVP func,void *arg)
{
  if (func == NULL)
    return ;

  if (funcIdx == funcCount)
    {
      funcCount += 10 ;
      if (funcs == NULL)
        {
          funcs = (PFIVP *) malloc (sizeof (PFIVP) * funcCount);
          args = (void **) malloc (sizeof (void *) * funcCount) ;
        }
      else
        {
          funcs = (PFIVP *) realloc (funcs,sizeof (PFIVP) * funcCount);
          args = (void **) realloc (args,sizeof (void *) * funcCount) ;
        }
    }

  args [funcIdx] = arg ;
  funcs [funcIdx++] = func ;
  
}


void configRemoveLoadCallback (PFIVP func)
{
  int i, j ;

  for (i = 0 ; i < funcIdx ; i++)
    if (funcs [i] == func)
      break ;

  for (j = i ; j < funcIdx - 1 ; j++)
    {
      funcs [j] = funcs [j + 1] ;
      args [j] = args [j + 1] ;
    }

  if (funcIdx > 1 && i < funcIdx)
    {
      funcs [i - 2] = funcs [i - 1] ;
      args [i - 2] = args [i - 1] ;
    }

  if (funcIdx > 0 && i < funcIdx)
    funcIdx-- ;
}


static int doCallbacks (void)
{
  int i ;
  int rval = 1 ;
  
  for (i = 0 ; i < funcIdx ; i++)
    if (funcs [i] != NULL)
      rval = (funcs[i](args [i]) && rval) ;

  return rval ;
}





static char *key ;
%}

%union{
    scope *scp ;
    value *val ;
    char *name ;
    int integer ;
    double real ;
    char *string ;
    char chr ;
}

%token PEER
%token GROUP
%token IVAL
%token RVAL
%token NAME
%token XSTRING
%token SCOPE
%token COLON
%token LBRACE
%token RBRACE
%token TRUEBVAL
%token FALSEBVAL
%token CHAR
%token WORD
%token IP_ADDRESS

%type <integer> IVAL
%type <real> RVAL
%type <string> XSTRING
%type <chr> CHAR
%type <name> TRUEBVAL FALSEBVAL WORD

%%
input: { 	
		lineCount = 1 ;
		addScope (NULL,"",newScope ("")) ;
		topScope = currScope ;
	} entries { if (!doCallbacks()) YYABORT ; } ;

scope: entries ;

entries:	
	| entries entry
	| entries error {
		errbuff = malloc (strlen(SYNTAX_ERROR) + 12) ;
		sprintf (errbuff,SYNTAX_ERROR,lineCount) ;
		YYABORT ;
	}
	;

entry:	PEER WORD LBRACE {
		errbuff = addScope (currScope,$2,newScope ("peer")) ;
                free ($2) ;
		if (errbuff != NULL) YYABORT ;
	} scope RBRACE {
		currScope = currScope->parent ;
	}
	| GROUP WORD LBRACE {
		errbuff = addScope (currScope,$2,newScope ("group")) ;
                free ($2) ;
		if (errbuff != NULL) YYABORT ;
	} scope RBRACE {
		currScope = currScope->parent ;
	}
	| WORD WORD LBRACE {
		errbuff = malloc (strlen(UNKNOWN_SCOPE_TYPE) + 15 +
					  strlen ($1)) ;
		sprintf (errbuff,UNKNOWN_SCOPE_TYPE,lineCount,$1) ;
                free ($1) ;
                free ($2) ;
		YYABORT ;
	}
	| WORD { 
		if ((errbuff = keyOk($1)) != NULL) {
			YYABORT ;
		} else
			key = $1 ;
	} COLON value ;

value:	WORD {
		if ((errbuff = addString (currScope, key, $1)) != NULL)
			YYABORT ;
                free (key) ;
                free ($1) ;
	}
	| IVAL {
		if ((errbuff = addInteger(currScope, key, $1)) != NULL)
			YYABORT; 
                free (key) ;
	}
	| TRUEBVAL {
		if ((errbuff = addBoolean (currScope, key, 1)) != NULL)
			YYABORT ; 
                free (key) ;
                free ($1) ;
	}
	| FALSEBVAL {
		if ((errbuff = addBoolean (currScope, key, 0)) != NULL)
			YYABORT ; 
                free (key) ;
                free ($1) ;
	}
	| RVAL {
		if ((errbuff = addReal (currScope, key, $1)) != NULL)
			YYABORT ; 
                free (key) ;
	}
	| XSTRING { 
		if ((errbuff = addString (currScope, key, $1)) != NULL)
			YYABORT;
                free (key) ;
	}
	| CHAR {
		if ((errbuff = addChar (currScope, key, $1)) != NULL)
			YYABORT ;
                free (key) ;
        }
;
	
%%

int yyerror (const char *s)
{
#undef FMT
#define FMT "line %d: %s"
  
  errbuff = malloc (strlen (s) + strlen (FMT) + 20) ;
  sprintf (errbuff,FMT,lineCount,s) ;

  return 0 ;
}

int yywrap (void)
{
  return 1 ;
}

extern FILE *yyin ;
extern int yydebug ;

#define NO_INHERIT 0


#if ! defined (WANT_MAIN)

struct peer_table_s
{
    char *peerName ;
    value *peerValue ;
} ;

static struct peer_table_s *peerTable ;
static int peerTableCount ;
static int peerTableIdx ;

void configCleanup (void)
{
  int i ;

  for (i = 0 ; i < peerTableIdx ; i++)
    free (peerTable[i].peerName) ;
  free (peerTable) ;
  
  freeScopeTree (topScope);
  free (funcs) ;
  free (args) ;
}
  

int buildPeerTable (FILE *fp, scope *s)
{
  int rval = 1 ;
  int i, j ;

  for (i = 0 ; i < s->value_idx ; i++)
    {
      if (ISSCOPE (s->values[i]) && ISPEER (s->values[i]))
        {
          for (j = 0 ; j < peerTableIdx ; j++)
            {
              if (strcmp (peerTable[j].peerName,s->values[i]->name) == 0)
                {
                  logOrPrint (LOG_ERR,fp,DUP_PEER_NAME,peerTable[j].peerName) ;
                  rval = 0 ;
                  break ;
                }
            }

          if (j == peerTableIdx)
            {
              if (peerTableCount == peerTableIdx) 
                {
                  peerTableCount += 10 ;
                  if (peerTable == NULL)
                    peerTable = ALLOC(struct peer_table_s,peerTableCount) ;
                  else
                    peerTable = REALLOC (peerTable,struct peer_table_s,
                                         peerTableCount) ;
                }
  
              peerTable[peerTableIdx].peerName = strdup (s->values[i]->name);
              peerTable[peerTableIdx].peerValue = s->values[i] ;
              peerTableIdx++ ;
            }
        }
      else if (ISSCOPE (s->values[i]))
        rval = (buildPeerTable (fp,s->values[i]->v.scope_val) && rval) ;
    }

  return rval ;  
}


/* read the config file. Any errors go to errorDest if it is non-NULL,
   otherwise they are syslogged. If justCheck is true then return after
   parsing */
static int inited = 0 ;
int readConfig (const char *file, FILE *errorDest, int justCheck, int dump)
{
  scope *oldTop = topScope ;
  FILE *fp ;
  int rval ;

  if (!inited)
    {
      inited = 1 ;
      yydebug = (getenv ("YYDEBUG") == NULL ? 0 : 1) ;
      if (yydebug)
        atexit (configCleanup) ;
    }

  if (file == NULL || strlen (file) == NULL || !fileExistsP (file))
    {
      logOrPrint (LOG_ERR,errorDest,NOSUCH_CONFIG, file ? file : "(null)") ;
      dprintf (1,"No such config file: %s\n", file ? file : "(null)") ;
      exit (1) ;
    }

  if ((fp = fopen (file,"r")) == NULL)
    {
      logOrPrint (LOG_ERR,errorDest,CFG_FOPEN_FAILURE, file) ;
      exit (1) ;
    }

  logOrPrint (LOG_NOTICE,errorDest,"loading %s", file) ;

  yyin = fp ;

  topScope = NULL ;

  rval = yyparse () ;

  fclose (fp) ;
  
  if (rval != 0)                /* failure */
    {
      freeScopeTree (topScope) ;
      if (justCheck)
        freeScopeTree (oldTop) ;
      else
        topScope = oldTop ;
      topScope = NULL ;

      if (errbuff != NULL)
        {
          if (errorDest != NULL)
            fprintf (errorDest,CONFIG_PARSE_FAILED,"",errbuff) ;
          else
            syslog (LOG_ERR,CONFIG_PARSE_FAILED,"ME ",errbuff) ;
          
          free (errbuff) ;
        }
      
      return 0 ;
    }
  
  if (dump)
    {
      fprintf (errorDest ? errorDest : stderr,"Parsed config file:\n") ;
      printScope (errorDest ? errorDest : stderr,topScope,-5) ;
      fprintf (errorDest ? errorDest : stderr,"\n") ;
    }
  
  if (justCheck)
    {
      freeScopeTree (topScope) ;
      freeScopeTree (oldTop) ;

      topScope = NULL ;
    }
  else
    {
      for (peerTableIdx-- ; peerTableIdx >= 0 ; peerTableIdx--)
        {
          free (peerTable [peerTableIdx].peerName) ;
          peerTable [peerTableIdx].peerName = NULL ;
          peerTable [peerTableIdx].peerValue = NULL ;
        }
      peerTableIdx = 0 ;
      
      if (!buildPeerTable (errorDest,topScope))
        logAndExit (1,"Failed to build list of peers") ;
    }
  
  return 1 ;
}


value *getNextPeer (int *cookie)
{
  value *rval ;

  if (*cookie < 0 || *cookie >= peerTableIdx)
    return NULL ;

  rval = peerTable[*cookie].peerValue ;

  (*cookie)++ ;

  return rval ;
}


value *findPeer (const char *name)
{
  value *v = NULL ;
  int i ;

  for (i = 0 ; i < peerTableIdx ; i++)
    if (strcmp (peerTable[i].peerName,name) == 0)
      {
        v = peerTable[i].peerValue ;
        break ;
      }
  
  return v ;
}

#endif

#if defined (WANT_MAIN)
int main (int argc, char **argv) {
  if ( yyparse() )
    printf ("parsing failed: %s\n",errbuff ? errbuff : "NONE") ;
  else
    {
      printScope (stdout,topScope,-5) ;

      if (argc == 3)
        {
#if 0
          printf ("Looking for %s of type %s: ",argv[2],argv[1]) ;
          if (strncmp (argv[1],"int",3) == 0)
            {
              int i = 0 ;
          
              if (!getInteger (topScope,argv[2],&i))
                printf ("wasn't found.\n") ;
              else
                printf (" %d\n",i) ;
            }
          else if (strncmp (argv[1],"real",4) == 0)
            {
              double d = 0.0 ;

              if (!getReal (topScope,argv[2],&d))
                printf ("wasn't found.\n") ;
              else
                printf (" %0.5f\n",d) ;
            }
#else
          value *v = findValue (topScope,argv[1],1) ;

          if (v == NULL)
            printf ("Can't find %s\n",argv[1]) ;
          else
            {
              long ival = 987654 ;
              
              if (getInteger (v->v.scope_val,argv[2],&ival,1))
                printf ("Getting %s : %ld",argv[2],ival) ;
              else
                printf ("Name is not legal: %s\n",argv[2]) ;
            }
#endif
        }
      else if (argc == 2)
        {
#if 1
          value *v = findValue (topScope,argv[1],1) ;

          if (v == NULL)
            printf ("Can't find %s\n",argv[1]) ;
          else
            {
              printf ("Getting %s : ",argv[1]) ;
              printValue (stdout,v,0) ;
            }
#else
          if (findScope (topScope,argv[1],1) == NULL)
            printf ("Can't find the scope of %s\n",argv[1]) ;
#endif
        }
    }
  
  freeScopeTree (topScope) ;

  return 0 ;
}
#endif /* defined (WANT_MAIN) */


