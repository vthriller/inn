/* -*- c -*-
 *
 * Author:      Christophe Wolfhugel <wolf@pasteur.fr>
 *		(although he wouldn't recognise it anymore so don't blame him)
 * File:        perl.c
 * RCSId:       $Id$
 * Description: Perl hooks for nnrpd.
 * 
 */

#if ! defined (lint)
static const char *rcsid = "$Id$" ;
static void use_rcsid (const char *rid) {   /* Never called */
  use_rcsid (rcsid) ; use_rcsid (rid) ;
}
#endif


#include <stdio.h>
#include <sys/types.h>
#include <netinet/in.h>
#include "configdata.h"
#include "clibrary.h"
#include "paths.h"
#include "post.h"
#include <syslog.h> 
#include "macros.h"
#include "nntp.h"
#include "nnrpd.h"

#if defined(DO_PERL)

#if defined (DO_NEED_BOOL)
typedef enum { false = 0, true = 1 } bool;
#endif

#include <EXTERN.h>
#include <perl.h>
#include <XSUB.h>

/* Perl 5.004 didn't define ERRSV and PL_na was called na. */
#ifndef ERRSV
# define ERRSV GvSV(errgv)
# define PL_sv_undef sv_undef
# define PL_na na
#endif

extern BOOL PerlFilterActive;
extern HEADER	Table[], *EndOfTable;
extern char LogName[];
extern char PERMuser[];

extern char **OtherHeaders;
extern int OtherCount;
extern BOOL HeadersModified;
static int HeaderLen;

/* #define DEBUG_MODIFY only if you want to see verbose outout */
#ifdef DEBUG_MODIFY
static FILE *flog;
void dumpTable(char *msg);
#endif /* DEBUG_MODIFY */

char *HandleHeaders(char *article)
{
   dSP;
   HEADER	*hp;
   HV		*hdr;
   SV           *body;
   int		rc;
   char		*p;
   static char	buf[256];
   register int   i;
   register char *s,*t;
   HE            *scan;
   SV            *modswitch;
   int            OtherSize;

   if (!PerlFilterActive)
       return NULL; /* not really necessary */

#ifdef DEBUG_MODIFY
   if ((flog = fopen("/var/news/log/nnrpdperlerrror","a+")) == NULL) {
     syslog(L_ERROR,"Whoops. Can't open error log: %m");
   }
#endif /* DEBUG_MODIFY */
   
   ENTER ;
   SAVETMPS ;
   
   /* Create the Perl Hash */
   hdr = perl_get_hv("hdr", TRUE);
   for (hp = Table; hp < EndOfTable; hp++) {
      if (hp->Value)
         hv_store(hdr, (char *) hp->Name, strlen(hp->Name), newSVpv(hp->Value, 0), 0);
   }
   
   /* Also store other headers */
   OtherSize = OtherCount;
   for (i = 0; i < OtherCount; i++) {
	p = OtherHeaders[i];
        if (p == NULL) {
          syslog (L_ERROR,"Null header number %d copying headers for Perl",i);
          continue;
        }
        s = strchr(p,':');
        if (s == NULL) {
          syslog (L_ERROR,"Bad header copying headers for Perl: '%s'",p);
          continue;
        }
        s++;
        hv_store(hdr, p, (s - p) - 1, newSVpv(s, 0), 0);
   }
   /* Store user */
   sv_setpv(perl_get_sv("user",TRUE), PERMuser);
   
   /* Store body */
   body = perl_get_sv("body", TRUE);
   sv_setpv(body, article);

   /* Call the filtering function */
   rc = perl_call_argv("filter_post", G_EVAL|G_SCALAR, NULL);

   SPAGAIN;

   /* Restore headers */
   modswitch = perl_get_sv("modify_headers",FALSE);
   HeadersModified = FALSE;
   if (SvTRUE(modswitch)) {
     HeadersModified = TRUE;
     HeaderLen = 0;
     i = 0;

#ifdef DEBUG_MODIFY     
     dumpTable("Before mod");
#endif /* DEBUG_MODIFY */

     hv_iterinit(hdr);
     while ((scan = hv_iternext(hdr)) != NULL) {
       register int x;
       
       /* Get the values */
       p = HePV(scan, PL_na);  
       s = SvPV(HeVAL(scan), PL_na);
#ifdef DEBUG_MODIFY     
       fprintf(flog,"Hash iter: '%s','%s'\n",p,s);
#endif /* DEBUG_MODIFY */

       /* See if it's a table header */
       for (hp = Table; hp < EndOfTable; hp++) {
         if (caseEQn(p, hp->Name, hp->Size)) {
           hp->Value = COPY(s);
           HeaderLen += strlen(s) + hp->Size + 3;
           break;
         }
       }
       if (hp != EndOfTable) continue;
       
       /* Add to other headers */
       if (i >= OtherSize - 1) {
         OtherSize += 20; 
         RENEW(OtherHeaders, char*, OtherSize);
       }
       x = strlen(p) + strlen(s) + 3;
       t = NEW(char, x);
       sprintf(t,"%s: %s",p,s);
       OtherHeaders[i++] = t;
       HeaderLen += x; 
     }
     OtherCount = i;
#ifdef DEBUG_MODIFY
     dumpTable("After Mod");
#endif /* DEBUG_MODIFY */
   }

   hv_undef (hdr);
   sv_setsv (body, &PL_sv_undef);

   buf [0] = '\0' ;
   
   if (SvTRUE(ERRSV))     /* check $@ */ {
       syslog (L_ERROR,"Perl function filter_post died: %s",
               SvPV(ERRSV, PL_na)) ;
       POPs ;
       PerlFilter (FALSE) ;
   } else if (rc == 1) {
       p = POPp;
       if (p != NULL && *p != '\0') {
           strncpy(buf, p, sizeof(buf) - 1);
           buf[sizeof(buf) - 1] = '\0';
       }
   }

   FREETMPS ;
   LEAVE ;
   
   if (buf[0] != '\0') 
      return buf ;
   return NULL;
}

int perlConnect(char *ClientHost, char *ClientIP, char *accesslist) {
    dSP;
    HV              *attribs;
    int             rc;
    SV              *sv;
    char            *p;
    int             code;
    
    if (!PerlFilterActive)
	return NNTP_ACCESS_VAL;

    ENTER;
    SAVETMPS;
    attribs = perl_get_hv("attributes", TRUE);
    hv_store(attribs, "type", 4, newSVpv("connect", 0), 0);
    hv_store(attribs, "hostname", 8, newSVpv(ClientHost, 0), 0);
    hv_store(attribs, "ipaddress", 9, newSVpv(ClientIP, 0), 0);
    
    PUSHMARK(SP);
    rc = perl_call_pv("authenticate", G_EVAL|G_ARRAY);

    SPAGAIN;

    if (rc == 0 ) { /* Error occured, same as checking $@ */
	syslog(L_ERROR, "Perl function authenticate died: %s",
	       SvPV(ERRSV, PL_na));
	Reply("%d Internal Error (1).  Goodbye\r\n", NNTP_ACCESS_VAL);
	ExitWithStats(1);
    }

    if (rc != 4) {
	syslog(L_ERROR, "Perl function authenticate returned wrong number of results: %d", rc);
	Reply("%d Internal Error (2).  Goodbye\r\n", NNTP_ACCESS_VAL);
	ExitWithStats(1);
    }

    p = POPp;
    strcpy(accesslist, p); 
    sv = POPs; PERMcanpost = SvTRUE(sv);
    sv = POPs; PERMcanread = SvTRUE(sv); 
    code = POPi;

    if ((code == NNTP_POSTOK_VAL) || (code == NNTP_NOPOSTOK_VAL))
	code = PERMcanpost ? NNTP_POSTOK_VAL : NNTP_NOPOSTOK_VAL;

    if (code == NNTP_AUTH_NEEDED_VAL) 
	PERMneedauth = TRUE;

    hv_undef(attribs);

    PUTBACK;
    FREETMPS;
    LEAVE;
    
    return code;
}

int perlAuthInit(char *ClientHost, char *ClientIP, char *accesslist) {
    dSP;
    int             rc;
    
    if (!PerlFilterActive)
	return NNTP_ACCESS_VAL;

    ENTER;
    SAVETMPS;
    PUSHMARK(SP);
    
    if (perl_get_cv("auth_init", 0) != NULL) 
	rc = perl_call_pv("auth_init", G_EVAL|G_DISCARD);

    SPAGAIN;


    if (SvTRUE(ERRSV))     /* check $@ */ {
	syslog(L_ERROR, "Perl function authenticate died: %s",
	       SvPV(ERRSV, PL_na));
	Reply("%d Internal Error (1).  Goodbye\r\n", NNTP_ACCESS_VAL);
	ExitWithStats(1);
    }

    while (rc--) {
	POPs;
    }

    PUTBACK;
    FREETMPS;
    LEAVE;
    
}

int perlAuthenticate(char *ClientHost, char *ClientIP, char *user, char *passwd, char *accesslist) {
    dSP;
    HV              *attribs;
    int             rc;
    SV              *sv;
    char            *p;
    int             code;
    
    if (!PerlFilterActive)
	return NNTP_ACCESS_VAL;

    ENTER;
    SAVETMPS;
    attribs = perl_get_hv("attributes", TRUE);
    hv_store(attribs, "type", 4, newSVpv("authenticate", 0), 0);
    hv_store(attribs, "hostname", 8, newSVpv(ClientHost, 0), 0);
    hv_store(attribs, "ipaddress", 9, newSVpv(ClientIP, 0), 0);
    hv_store(attribs, "username", 8, newSVpv(user, 0), 0);
    hv_store(attribs, "password", 8, newSVpv(passwd, 0), 0);
    
    PUSHMARK(SP);
    rc = perl_call_pv("authenticate", G_EVAL|G_ARRAY);

    SPAGAIN;

    if (rc == 0 ) { /* Error occured, same as checking $@ */
	syslog(L_ERROR, "Perl function authenticate died: %s",
	       SvPV(ERRSV, PL_na));
	Reply("%d Internal Error (1).  Goodbye\r\n", NNTP_ACCESS_VAL);
	ExitWithStats(1);
    }

    if (rc != 4) {
	syslog(L_ERROR, "Perl function authenticate returned wrong number of results: %d", rc);
	Reply("%d Internal Error (2).  Goodbye\r\n", NNTP_ACCESS_VAL);
	ExitWithStats(1);
    }

    p = POPp;
    strcpy(accesslist, p); 
    sv = POPs; PERMcanpost = SvTRUE(sv);
    sv = POPs; PERMcanread = SvTRUE(sv); 
    code = POPi;

    if ((code == NNTP_POSTOK_VAL) || (code == NNTP_NOPOSTOK_VAL))
	code = PERMcanpost ? NNTP_POSTOK_VAL : NNTP_NOPOSTOK_VAL;

    if (code == NNTP_AUTH_NEEDED_VAL) 
	PERMneedauth = TRUE;

    hv_undef(attribs);

    PUTBACK;
    FREETMPS;
    LEAVE;
    
    return code;
}

#ifdef DEBUG_MODIFY
void
dumpTable (msg)
char *msg;
{
      HEADER        *hp;
      register int   i;

      fprintf(flog,"===BEGIN TABLE DUMP: %s\n",msg);
      
      for (hp = Table; hp < EndOfTable; hp++) {
        fprintf(flog," Name: '%s'",hp->Name); fflush(flog);
        fprintf(flog," Size: '%d'",hp->Size); fflush(flog);
        fprintf(flog," Value: '%s'\n",((hp->Value == NULL) ? "(NULL)" : hp->Value)); fflush(flog);
      }

      for (i=0; i<OtherCount; i++) {
        fprintf(flog,"Extra[%02d]: %s\n",i,OtherHeaders[i]);
      }
      fprintf(flog,"===END TABLE DUMP: %s\n",msg);
}
#endif /* DEBUG_MODIFY */

#endif /* defined(DO_PERL) */

