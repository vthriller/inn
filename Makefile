##  $Id$

include Makefile.global

##  All installation directories except for $(PATHRUN), which has a
##  different mode than the rest.
INSTDIRS      = $(PATHNEWS) $(PATHBIN) $(PATHAUTH) $(PATHAUTHRESOLV) \
		$(PATHAUTHPASSWD) $(PATHCONTROL) $(PATHFILTER) \
		$(PATHRNEWS) $(PATHDB) $(PATHDOC) $(PATHETC) $(PATHLIB) \
		$(PATHMAN) $(MAN1) $(MAN3) $(MAN5) $(MAN8) $(PATHSPOOL) \
		$(PATHTMP) $(PATHARCHIVE) $(PATHARTICLES) $(PATHINCOMING) \
		$(PATHINBAD) $(PATHTAPE) $(PATHOVERVIEW) $(PATHOUTGOING) \
		$(PATHLOG) $(PATHLOG)/OLD

##  LIBDIRS are built before PROGDIRS, make update runs in all UPDATEDIRS,
##  and make install runs in all ALLDIRS.  Nothing runs in test except the
##  test target itself and the clean targets.  Currently, include is built
##  before anything else but nothing else runs in it except clean targets.
LIBDIRS     = lib storage
PROGDIRS    = innd nnrpd innfeed expire frontends backends authprogs scripts
UPDATEDIRS  = $(LIBDIRS) $(PROGDIRS) doc
ALLDIRS     = $(UPDATEDIRS) samples site
CLEANDIRS   = $(ALLDIRS) include tests

##  The directory name and tar file to use when building a release.
TARDIR      = inn-$(VERSION)
TARFILE     = $(TARDIR).tar

##  DISTDIRS gets all directories from the MANIFEST, and DISTFILES gets all
##  regular files.  Anything not listed in the MANIFEST will not be included
##  in a distribution.  These are arguments to sed.
DISTDIRS    = -e 1,2d -e '/(Directory)/!d' -e 's/ .*//' -e 's;^;$(TARDIR)/;'
DISTFILES   = -e 1,2d -e '/(Directory)/d' -e 's/ .*//'


##  Major target -- build everything.  Rather than just looping through
##  all the directories, use a set of parallel rules so that make -j can
##  work on more than one directory at a time.
all: all-include all-libraries all-programs
	cd doc     && $(MAKE) all
	cd samples && $(MAKE) all
	cd site    && $(MAKE) all

all-libraries:	all-lib all-storage

all-include:			; cd include   && $(MAKE) all
all-lib:	all-include	; cd lib       && $(MAKE) all
all-storage:	all-include	; cd storage   && $(MAKE) all

all-programs:	all-innd all-nnrpd all-innfeed all-expire all-frontends \
		all-backends all-authprogs all-scripts

all-authprogs:	all-lib		; cd authprogs && $(MAKE) all
all-backends:	all-libraries	; cd backends  && $(MAKE) all
all-expire:	all-libraries	; cd expire    && $(MAKE) all
all-frontends:	all-libraries	; cd frontends && $(MAKE) all
all-innd:	all-libraries	; cd innd      && $(MAKE) all
all-innfeed:	all-libraries	; cd innfeed   && $(MAKE) all
all-nnrpd:	all-libraries	; cd nnrpd     && $(MAKE) all
all-scripts:			; cd scripts   && $(MAKE) all


##  If someone tries to run make before running configure, tell them to run
##  configure first.
Makefile.global:
	@echo 'Run ./configure before running make.  See INSTALL for details.'
	@exit 1


##  Installation rules.  make install installs everything; make update only
##  updates the binaries, scripts, and documentation and leaves config
##  files alone.
install: directories
	@for D in $(ALLDIRS) ; do \
	    echo '' ; \
	    cd $$D && $(MAKE) install || exit 1 ; cd .. ; \
	done
	@echo ''
	@echo 'Do not forget to update your cron entries, and also run'
	@echo 'makedbz if you need to.  If this is a first-time installation'
	@echo 'a minimal active file has been installed.  You will need to'
	@echo 'touch history and run "makedbz -i" to initialize the history'
	@echo 'database.  See INSTALL for more information.'
	@echo ''

directories:
	@chmod +x support/install-sh
	for D in $(INSTDIRS) ; do \
	    support/install-sh $(OWNER) -m 0755 -d $$D ; \
	done
	support/install-sh $(OWNER) -m 0750 -d $(PATHRUN)

update: 
	@chmod +x support/install-sh
	@for D in $(UPDATEDIRS) ; do \
	    echo '' ; \
	    cd $$D && $(MAKE) install || exit 1 ; cd .. ; \
	done

##  Install a certificate for TLS/SSL support.
cert:
	$(SSLBIN)/openssl req -new -x509 -nodes \
	    -out $(PATHLIB)/cert.pem -days 366 \
	    -keyout $(PATHLIB)/cert.pem
	chown $(NEWSUSER) $(PATHLIB)/cert.pem
	chgrp $(NEWSGROUP) $(PATHLIB)/cert.pem
	chmod 640 $(PATHLIB)/cert.pem


##  Cleanup targets.  clean deletes all compilation results but leaves the
##  configure results.  distclean or clobber removes everything not part of
##  the distribution tarball.
clean:
	@for D in $(CLEANDIRS) ; do \
	    echo '' ; \
	    cd $$D && $(MAKE) clean || exit 1 ; cd .. ; \
	done

clobber realclean distclean:
	@for D in $(CLEANDIRS) ; do \
	    echo '' ; \
	    cd $$D && $(MAKE) $(FLAGS) clobber && cd .. ; \
	done
	@echo ''
	rm -rf $(TARDIR)
	rm -f inn*.tar.gz CHANGES ChangeLog LIST.* TAGS tags
	rm -f config.cache config.log config.status libtool
	rm -f support/fixscript Makefile.global


##  Other generic targets.
depend tags ctags profiled:
	@for D in $(ALLDIRS) ; do \
	    echo '' ; \
	    cd $$D && $(MAKE) $@ || exit 1 ; cd .. ; \
	done

TAGS etags:
	etags */*.c */*.h */*/*.c */*/*.h


##  Run the test suite.
check test tests:
	cd tests && $(MAKE) test


##  For maintainers, build the entire source base with warnings enabled.
warnings:
	$(MAKE) COPT='$(WARNINGS)' all


##  Make a release.  We create a release by recreating the directory
##  structure and then copying over all files listed in the MANIFEST.  If it
##  isn't in the MANIFEST, it doesn't go into the release.  We also update
##  the version information in Makefile.global.in to remove the prerelease
##  designation and update all timestamps to the date the release is made.
release: ChangeLog
	rm -rf $(TARDIR)
	rm -f inn*.tar.gz
	mkdir $(TARDIR)
	for d in `sed $(DISTDIRS) MANIFEST` ; do mkdir -p $$d ; done
	for f in `sed $(DISTFILES) MANIFEST` ; do \
	    cp $$f $(TARDIR)/$$f || exit 1 ; \
	done
	sed 's/= CVS prerelease/=/' < Makefile.global.in \
	    > $(TARDIR)/Makefile.global.in
	cp ChangeLog $(TARDIR)
	find $(TARDIR) -type f -print | xargs touch -t `date +%m%d%H%M.%S`
	tar cf $(TARFILE) $(TARDIR)
	$(GZIP) -9 $(TARFILE)

##  Generate the ChangeLog using support/mkchangelog.  This should only be
##  run by a maintainer since it depends on cvs log working and also
##  requires cvs2cl be available somewhere.
ChangeLog:
	support/mkchangelog


##  Check the MANIFEST against the files present in the current tree,
##  building a list with find and running diff between the lists.
check-manifest:
	sed -e 1,2d -e 's/ .*//' MANIFEST > LIST.manifest
	find . -print | sed -e 's/^\.\///' -e /CVS/d | sort > LIST.real
	diff -u LIST.manifest LIST.real
