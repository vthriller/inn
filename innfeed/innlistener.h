/*  $Id$
**
**  The public interface to the InnListener class.
**
**  Written by James Brister <brister@vix.com>
**  Copyright 1996 by the Internet Software Consortium
**
**  For license terms, see the end of this file.
**
**  The public interface of the things that listens to commands from INN. It
**  receives lines of the form:
**
**      filename msgid peer1 peer2 peer3
**
**  and turns them into Article objects and hands those Articles off to the
**  Host objects.
*/

#if ! defined ( innlistener_h__ )
#define innlistener_h__

#include <stdio.h>

#include "misc.h"


extern InnListener mainListener ;

/* Initialization of the InnListener object. If it fails then returns
  NULL. ENDPOINT is the endpoint where the article info will come
  from. A dummy listener exists when processing backlog files and is
  there just to help drive the process. */
InnListener newListener (EndPoint endp, bool isDummy, bool dynamicPeers) ;

/* print some useful debugging information about the Listener and all its
 * Hosts and all their Connecitons/Article/Buffers etc. to the given FILE.
 */
void gPrintListenerInfo (FILE *fp, unsigned int indentAmt) ;
void printListenerInfo (InnListener listener, FILE *fp, unsigned int indentAmt) ;

/* Called by the Host when it is about to delete itself */
unsigned int listenerHostGone (InnListener listener, Host host) ;

/* Called to hook up the given Host to the Listener. */
bool listenerAddPeer (InnListener listener, Host hostObj) ;

/* true if the listener is a dummy. */
bool listenerIsDummy (InnListener listener) ;

/*
 * This gets called to stop accepting new articles from innd. Typically
 * called by the signal handler, or when the listener gets EOF on its input
 * (in channel mode)
 */
void shutDown (InnListener cxn) ;

/* Callback fired after config file is loaded */
int listenerConfigLoadCbk (void *data) ;

  /* stop a specific host. */
void shutDownHost (InnListener cxn, const char *peerName) ;

  /* Called by the Host when it has nothing to do (so it can be shut down
     if necessary). */
void listenerHostIsIdle (InnListener listener, Host host) ;

void openInputFile (void) ;

void openDroppedArticleFile (void) ;
void closeDroppedArticleFile (void) ;

void listenerLogStatus (FILE *fp) ;

#endif /* innlistener_h__ */

/*
**  Copyright 1996 by the Internet Software Consortium
**
**  Permission to use, copy, modify, and distribute this software for any
**  purpose with or without fee is hereby granted, provided that the above
**  copyright notice and this permission notice appear in all copies.
**
**  THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM
**  DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL
**  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS.  IN NO EVENT SHALL
**  INTERNET SOFTWARE CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT,
**  OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF
**  USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
**  OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
**  PERFORMANCE OF THIS SOFTWARE.
*/