# -*- tcl -*-
#
# Author:       James Brister <brister@vix.com> -- berkeley-unix --
# Start Date:   Sat, 24 Aug 1996 23:45:34 +0200
# Project:      INN
# File:         innshellvars.tcl
# RCSId:        $Id$
# Description:  Set up any and all variables that an INN tcl script
#               might need.


##  =()<set inn_active "@<_PATH_ACTIVE>@">()=
set inn_active "/var/news/etc/active"

##  =()<set inn_activetimes "@<_PATH_ACTIVETIMES>@">()=
set inn_activetimes "/var/news/etc/active.times"

##  =()<set inn_awk "@<_PATH_AWK>@">()=
set inn_awk "/usr/bin/awk"

##  =()<set inn_batch "@<_PATH_BATCHDIR>@">()=
set inn_batch "/var/news/spool/out.going"

##  =()<set inn_compress "@<_PATH_COMPRESS>@">()=
set inn_compress "/usr/bin/compress"

##  =()<set inn_controlprogs "@<_PATH_CONTROLPROGS>@">()=
set inn_controlprogs "/usr/news/bin/control"

##  =()<set inn_ctlfile "@<_PATH_CONTROLCTL>@">()=
set inn_ctlfile "/var/news/etc/control.ctl"

##  =()<scan "@<LOG_CYCLES>@" "%d" inn_cycles>()=
scan "3" "%d" inn_cycles

##  =()<set inn_daily "@<_PATH_LOCKS>@/LOCK.news.daily">()=
set inn_daily "/var/news/locks/LOCK.news.daily"

##  =()<set inn_inndf "@<INNWATCH_DF>@">()=
set inn_inndf "/bin/df"

##  =()<set inn_egrep "@<_PATH_EGREP>@">()=
set inn_egrep "/usr/bin/egrep"

##  =()<set inn_errlog "@<_PATH_ERRLOG>@">()=
set inn_errlog "/var/log/news/errlog"

##  =()<set inn_ctlwatch "@<_PATH_CTLWATCH>@">()=
set inn_ctlwatch "/var/news/etc/innwatch.ctl"

##  =()<set inn_have_uustat [ expr { "@<HAVE_UUSTAT>@" == "DO" ? 1 : 0 } ]>()=
set inn_have_uustat [ expr { "DO" == "DO" ? 1 : 0 } ]

##  =()<set inn_history "@<_PATH_HISTORY>@">()=
set inn_history "/var/news/etc/history"

##  =()<set inn_incoming "@<_PATH_SPOOLNEWS>@">()=
set inn_incoming "/var/news/spool/in.coming"

##  =()<set inn_inews "@<_PATH_INEWS>@">()=
set inn_inews "/usr/news/bin/inews"

##  =()<set inn_innconfval "@<_PATH_NEWSBIN>@/innconfval">()=
set inn_innconfval "/usr/news/bin/innconfval"

##  =()<set inn_innd "@<_PATH_INND>@">()=
set inn_innd "/usr/news/bin/innd"

##  =()<set inn_innddir "@<_PATH_INNDDIR>@">()=
set inn_innddir "/var/news/run"

##  =()<set inn_inndstart "@<_PATH_INNDSTART>@">()=
set inn_inndstart "/usr/news/bin/inndstart"

##  =()<set inn_innwatch "@<_PATH_NEWSBIN>@/innwatch">()=
set inn_innwatch "/usr/news/bin/innwatch"

##  =()<set inn_locks "@<_PATH_LOCKS>@">()=
set inn_locks "/var/news/locks"

##  =()<set inn_log "@<_PATH_LOGFILE>@">()=
set inn_log "/var/log/news/news"

##  =()<set inn_mail_badcontrols [ expr { "@<MAIL_BADCONTROLS>@" == "DO" ? 1 : 0 } ]>()=
set inn_mail_badcontrols [ expr { "DONT" == "DO" ? 1 : 0 } ]

##  =()<set inn_mailcmd "@<_PATH_MAILCMD>@">()=
set inn_mailcmd "/usr/bin/Mail"

##  =()<set inn_most_logs "@<_PATH_MOST_LOGS>@">()=
set inn_most_logs "/var/log/news"

##  =()<set inn_newactive "@<_PATH_NEWACTIVE>@">()=
set inn_newactive "/var/news/etc/active.tmp"

##  =()<set inn_newsbin "@<_PATH_NEWSBIN>@">()=
set inn_newsbin "/usr/news/bin"

##  =()<set inn_newscontrol "@<_PATH_NEWSCONTROL>@">()=
set inn_newscontrol "/var/news/run/control"

##  =()<set inn_newsfeeds "@<_PATH_NEWSFEEDS>@">()=
set inn_newsfeeds "/var/news/etc/newsfeeds"

##  =()<set inn_newsgroups "@<_PATH_NEWSGROUPS>@">()=
set inn_newsgroups "/var/news/etc/newsgroups"

##  =()<set inn_newslib "@<_PATH_NEWSLIB>@">()=
set inn_newslib "/var/news/etc"

##  =()<set inn_newsmaster "@<NEWSMASTER>@">()=
set inn_newsmaster "usenet"

##  =()<set inn_newsuser "@<NEWSUSER>@">()=
set inn_newsuser "news"

##  =()<set inn_nntpconnect "@<_PATH_NNTPCONNECT>@">()=
set inn_nntpconnect "/var/news/run/nntpin"

##  =()<set inn_oldactive "@<_PATH_OLDACTIVE>@">()=
set inn_oldactive "/var/news/etc/active.old"

##  =()<set inn_perl "@<_PATH_PERL>@">()=
set inn_perl "/usr/bin/perl"

##  =()<set inn_rnews "@<_PATH_RNEWS>@">()=
set inn_rnews "/usr/news/bin/rnews"

##  =()<set inn_sed "@<_PATH_SED>@">()=
set inn_sed "/usr/bin/sed"

##  =()<set inn_sort "@<_PATH_SORT>@">()=
set inn_sort "/usr/bin/sort"

##  =()<set inn_serverpid "@<_PATH_SERVERPID>@">()=
set inn_serverpid "/var/news/run/innd.pid"

##  =()<scan "@<INNWATCH_SLEEPTIME>@" "%d" inn_sleeptime>()=
scan "600" "%d" inn_sleeptime

##  =()<set inn_spool "@<_PATH_SPOOL>@">()=
set inn_spool "/var/news/spool/articles"

##  =()<set inn_overviewdir "@<_PATH_OVERVIEWDIR>@">()=
set inn_overviewdir "/var/news/spool/over.view"

##  =()<set inn_innwstatus "@<_PATH_INNWSTATUS>@">()=
set inn_innwstatus "/var/news/run/innwatch.status"

##  =()<set inn_tempsock [ eval exec basename @<_PATH_TEMPSOCK>@ | $inn_sed -e {s/XXXXXX$/*/} ]>()=
set inn_tempsock [ eval exec basename /var/news/run/ctlinndXXXXXX | $inn_sed -e {s/XXXXXX$/*/} ]

##  =()<set inn_tempsockdir [ exec echo @<_PATH_TEMPSOCK>@ | $inn_sed -e {s@/[^/]*$@@} ]>()=
set inn_tempsockdir [ exec echo /var/news/run/ctlinndXXXXXX | $inn_sed -e {s@/[^/]*$@@} ]

##  =()<set inn_want_pgpverify [ expr { "@<WANT_PGPVERIFY>@" == "DO" ? 1 : 0 } ]>()=
set inn_want_pgpverify [ expr { "DO" == "DO" ? 1 : 0 } ]

##  =()<set inn_watchpid "@<_PATH_WATCHPID>@">()=
set inn_watchpid "/var/news/run/innwatch.pid"

##  =()<set inn_z "@<_PATH_COMPRESSEXT>@">()=
set inn_z ".Z"

##  =()<set inn_perl_support [ expr { "@<PERL_SUPPORT>@" == "DO" ? 1 : 0 } ]>()=
set inn_perl_support [ expr { "DONT" == "DO" ? 1 : 0 } ]

##  =()<set inn_perl_startup_innd "@<_PATH_PERL_STARTUP_INND>@">()=
set inn_perl_startup_innd "/usr/news/bin/control/startup_innd.pl"

##  =()<set inn_perl_filter_innd "@<_PATH_PERL_FILTER_INND>@">()=
set inn_perl_filter_innd "/usr/news/bin/control/filter_innd.pl"

##  =()<set inn_perl_filter_nnrpd "@<_PATH_PERL_FILTER_NNRPD>@">()=
set inn_perl_filter_nnrpd "/usr/news/bin/control/filter_nnrpd.pl"

##  =()<set inn_tcl_support [ expr { "@<TCL_SUPPORT>@" == "DO" ? 1 : 0 } ]>()=
set inn_tcl_support [ expr { "DONT" == "DO" ? 1 : 0 } ]

## =()<set inn_path_tcl_startup "@<_PATH_TCL_STARTUP>@">()=
set inn_path_tcl_startup "/usr/news/bin/control/startup.tcl"

## =()<set inn_path_tcl_filter "@<_PATH_TCL_FILTER>@">()=
set inn_path_tcl_filter "/usr/news/bin/control/filter.tcl"

set inn_localgroups "$inn_newslib/localgroups"

if {[info exists env(TMPDIR)] != 1} {
    ##  =()<    set env(TMPDIR) "@<_PATH_TMP>@">()=
    set env(TMPDIR) "/var/tmp"
}
set inn_tmpdir $env(TMPDIR)

##  =()<set inn_expirectl "@<_PATH_EXPIRECTL>@">()=
set inn_expirectl "/var/news/etc/expire.ctl"

##  =()<set inn_newshome "@<_PATH_NEWSHOME>@">()=
set inn_newshome "/usr/news"

##  =()<set inn_archivedir "@<_PATH_ARCHIVEDIR>@">()=
set inn_archivedir "/var/news/spool/archive"

##  =()<set inn_badnews "@<_PATH_BADNEWS>@">()=
set inn_badnews "/var/news/spool/in.coming/bad"

#### =()<set inn_spoolnews "@<_PATH_SPOOLNEWS>@">()=
set inn_spoolnews "/var/news/spool/in.coming"

#### =()<set inn_spooltemp "@<_PATH_SPOOLTEMP>@">()=
set inn_spooltemp "/var/tmp"

##
set inn_newslbin "$inn_newshome/local"

##  =()<scan "@<NEWSUMASK>@" "%o" inn_umask>()=
scan "02" "%o" inn_umask

set env(PATH) "$inn_newslbin:$inn_newsbin:$env(PATH):/bin:/usr/bin:/usr/ucb"
