/*  $Revision$
**
**  The newnews command.
*/
#include "config.h"
#include "clibrary.h"

#include "inn/innconf.h"
#include "inn/messages.h"
#include "inn/wire.h"
#include "nnrpd.h"
#include "ov.h"
#include "cache.h"

#define GROUP_LIST_DELTA	10

static bool FindHeader(ARTHANDLE *art, const char **pp, const char **qp,
    const char* hdr, size_t hdrlen)
{
  const char *p, *p1, *q;
  bool Nocr = TRUE;

  p = wire_findheader(art->data, art->len, hdr);
  if (p == NULL)
    return false;
  q = p;
  for (p1 = NULL; p < art->data + art->len; p++) {
    if (p1 != NULL && *p1 == '\r' && *p == '\n') {
      Nocr = FALSE;
      break;
    }
    if (*p == '\n') {
      Nocr = TRUE;
      break;
    }
    p1 = p;
  }
  if (p >= art->data + art->len)
    return false;
  if (!Nocr)
    p = p1;

  *pp = p;
  *qp = q;
  return true;
}

/*
**  get Xref header
*/
static char *GetXref(ARTHANDLE *art) {
  const char	*p, *q;
  static char	buff[BIG_BUFFER];

  if (!FindHeader(art, &p, &q, "xref", sizeof("xref")))
    return NULL;
  return xstrndup(q, p - q);
}

/*
**  Split newsgroup list into array of newsgroups.  Return static pointer,
**  or NULL if there are no newsgroup.
*/
static char **GetGroups(char *p) {
  static int	size;
  static char	**list;
  int		i;
  char		*q;
  static char	*Xrefbuf = NULL;
  char		*Xref = p;

  if (size == 0) {
    size = GROUP_LIST_DELTA;
    list = NEW(char*, size + 1);
  }
  Xref = p;
  for (Xref++; *Xref == ' '; Xref++);
  if ((Xref = strchr(Xref, ' ')) == NULL)
    return NULL;
  for (Xref++; *Xref == ' '; Xref++);
  if (!Xrefbuf)
    Xrefbuf = NEW(char, BIG_BUFFER);
  strcpy(Xrefbuf, Xref);
  if ((q = strchr(Xrefbuf, '\t')) != NULL)
    *q = '\0';
  p = Xrefbuf;

  for (i = 0 ; ;i++) {
    while (ISWHITE(*p))
      p++;
    if (*p == '\0' || *p == '\n')
      break;

    if (i >= size - 1) {
      size += GROUP_LIST_DELTA;
      RENEW(list, char *, size + 1);
    }
    for (list[i] = p; *p && *p != '\n' && !ISWHITE(*p); p++) {
      if (*p == '/' || *p == ':')
	*p = '\0';
    }
    if (*p) *p++ = '\0';
  }
  list[i] = NULL;
  return i ? list : NULL;
}

static bool HaveSeen(bool AllGroups, char *group, char **groups, char **xrefs) {
  char *list[2];

  list[1] = NULL;
  for ( ; *xrefs; xrefs++) {
    list[0] = *xrefs;
    if ((!AllGroups && PERMmatch(groups, list)) && (!PERMspecified || (PERMspecified && PERMmatch(PERMreadlist, list)))) {
      if (!strcmp(*xrefs, group))
	return FALSE;
      else
	return TRUE;
    }
  }
  return FALSE;
}

static char **groups;

void
process_newnews(char *group, bool AllGroups, time_t date)
{
    char **xrefs;
    int count;
    void *handle;
    char *p;
    time_t arrived;
    ARTHANDLE *art = NULL;
    TOKEN token;
    char *data;
    int i, len;
    char *grplist[2];
    time_t now;

    grplist[0] = group;
    grplist[1] = NULL;
    if (PERMspecified && !PERMmatch(PERMreadlist, grplist))
	return;
    if (!AllGroups && !PERMmatch(groups, grplist))
	return;
    if (!OVgroupstats(group, &ARTlow, &ARThigh, &count, NULL))
	return;
    if ((handle = OVopensearch(group, ARTlow, ARThigh)) != NULL) {
	ARTNUM artnum;
	unsigned long artcount = 0;
	struct cvector *vector = NULL;

	if (innconf->nfsreader) {
	    time(&now);
	    /* move the start time back nfsreaderdelay seconds */
	    if (date >= innconf->nfsreaderdelay)
		date -= innconf->nfsreaderdelay;
	}
	while (OVsearch(handle, &artnum, &data, &len, &token, &arrived)) {
	    if (innconf->nfsreader && arrived + innconf->nfsreaderdelay > now)
		continue;
	    if (len == 0 || date > arrived)
		continue;

	    vector = overview_split(data, len, NULL, vector);
	    if (overhdr_xref == -1) {
		if ((art = SMretrieve(token, RETR_HEAD)) == NULL)
		    continue;
		p = GetXref(art);
		SMfreearticle(art);
		if (p == NULL)
		    continue;
	    } else {
		if (PERMaccessconf->nnrpdcheckart && 
		    !ARTinstorebytoken(token))
		    continue;
		/* We only care about the newsgroup list here, virtual
		 * hosting isn't relevant */
		p = overview_getheader(vector, overhdr_xref, OVextra);
	    }
	    xrefs = GetGroups(p);
	    free(p);
	    if (xrefs == NULL)
		continue;
	    if (HaveSeen(AllGroups, group, groups, xrefs))
		continue;
	    p = overview_getheader(vector, OVERVIEW_MESSAGE_ID, OVextra);
	    if (p == NULL)
		continue;

	    ++artcount;
	    cache_add(HashMessageID(p), token);
	    Printf("%s\r\n", p);
	    free(p);
	}
	OVclosesearch(handle);
	notice("%s newnews %s %lu", ClientHost, group, artcount);
	if (vector)
	    cvector_free(vector);
    }
}

/*
**  NEWNEWS newsgroups date time ["GMT"]
**  Return the Message-ID of any articles after the specified date
*/
void CMDnewnews(int ac, char *av[]) {
  char		*p, *q;
  char          *path;
  bool		AllGroups;
  char		line[BIG_BUFFER];
  time_t	date;
  QIOSTATE	*qp;
  int		i;
  bool          local;

  if (!PERMaccessconf->allownewnews) {
      Reply("%d NEWNEWS command disabled by administrator\r\n", NNTP_ACCESS_VAL);
      return;
  }

  if (!PERMcanread) {
      Reply("%s\r\n", NNTP_ACCESS);
      return;
  }

  /* Make other processes happier if someone uses NEWNEWS */
  if (innconf->nicenewnews > 0) {
      nice(innconf->nicenewnews);
      innconf->nicenewnews = 0;
  }

  snprintf(line, sizeof(line), "%s %s %s %s", av[1], av[2], av[3],
	   (ac >= 5 && (*av[4] == 'G' || *av[4] == 'U')) ? "GMT" : "local");
  notice("%s newnews %s", ClientHost, line);

  TMRstart(TMR_NEWNEWS);
  /* Optimization in case client asks for !* (no groups) */
  if (EQ(av[1], "!*")) {
      Reply("%s\r\n", NNTP_NEWNEWSOK);
      Printf(".\r\n");
      TMRstop(TMR_NEWNEWS);
      return;
  }

  /* Parse the newsgroups. */
  AllGroups = EQ(av[1], "*");
  if (!AllGroups && !NGgetlist(&groups, av[1])) {
      Reply("%d Bad newsgroup specifier %s\r\n", NNTP_SYNTAX_VAL, av[1]);
      TMRstop(TMR_NEWNEWS);
      return;
  }

  /* Parse the date. */
  local = !(ac > 4 && caseEQ(av[4], "GMT"));
  date = parsedate_nntp(av[2], av[3], local);
  if (date == (time_t) -1) {
      Reply("%d Bad date\r\n", NNTP_SYNTAX_VAL);
      TMRstop(TMR_NEWNEWS);
      return;
  }

  if (strcspn(av[1], "\\!*[?]") == strlen(av[1])) {
      /* optimise case - don't need to scan the active file pattern
       * matching */
      Reply("%s\r\n", NNTP_NEWNEWSOK);
      for (i = 0; groups[i]; ++i) {
	  process_newnews(groups[i], AllGroups, date);
      }
  } else {
      path = concatpath(innconf->pathdb, _PATH_ACTIVE);
      qp = QIOopen(path);
      if (qp == NULL) {
	  if (errno == ENOENT) {
	      Reply("%d Can't open active\r\n", NNTP_TEMPERR_VAL);
	  } else {
	      syswarn("%s cant fopen %s", ClientHost, path);
	      Reply("%d Can't open active\r\n", NNTP_TEMPERR_VAL);
	  }
	  free(path);
	  TMRstop(TMR_NEWNEWS);
	  return;
      }
      free(path);

      Reply("%s\r\n", NNTP_NEWNEWSOK);

      while ((p = QIOread(qp)) != NULL) {
	  for (q = p; *q != '\0'; q++) {
	      if (*q == ' ' || *q == '\t') {
		  *q = '\0';
		  break;
	      }
	  }
	  process_newnews(p, AllGroups, date);
      }
      (void)QIOclose(qp);
  }
  Printf(".\r\n");
  TMRstop(TMR_NEWNEWS);
}
