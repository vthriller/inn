##########################################################
# INN module for innreport (3.0.0 and more).
#
# Sample file tested with INN 1.8current and 1.5.1
#
# (c) 1997-1998 by Fabien Tassin <fta@oleane.net>
# version 2.1.9_7
##########################################################

# TODO: add the map file.

package innreport_inn;

my $MIN = 1E10;
my $MAX = -1;

my %ctlinnd = 
           ('a', 'addhist',     'D', 'allow',
	    'b', 'begin',       'c', 'cancel',
	    'u', 'changegroup', 'd', 'checkfile',
	    'e', 'drop',        'f', 'flush',
	    'g', 'flushlogs',   'h', 'go',
	    'i', 'hangup',      's', 'mode',
	    'j', 'name',        'k', 'newgroup',
	    'l', 'param',       'm', 'pause',
	    'v', 'readers',     't', 'refile',
	    'C', 'reject',      'o', 'reload',
	    'n', 'renumber',    'z', 'reserve',
	    'p', 'rmgroup',     'A', 'send',
	    'q', 'shutdown',    'B', 'signal',
	    'r', 'throttle',    'w', 'trace',
	    'x', 'xabort',      'y', 'xexec',
	    'E', 'logmode',	'F', 'feedinfo',
	    'T', 'filter',	'P', 'perl',);

# init innd timer
foreach ('article cancel', 'article control', 'article link',
	 'article write', 'history grep', 'history lookup',
	 'history sync', 'history write', 'idle', 'site send',
	 'perl filter')
{
  $innd_time_min{$_} = $MIN;
  $innd_time_max{$_} = $MAX;
  $innd_time_time{$_} = 0;   # to avoid a warning... Perl < 5.004
  $innd_time_num{$_} = 0;    # ...
}
$innd_time_times = 0;        # ...


# collect: Used to collect the data.
sub collect
{
  my ($day, $hour, $prog, $res, $left, $CASE_SENSITIVE) = @_;

  ########
  ## inn (from the "news" log file - not from "news.notice")
  ##
  if ($prog eq "inn")
  { 
    # accepted article
    if ($res =~ m/[\+cj]/o)
    {
      $hour =~ s/:.*$//o;
      $inn_flow{"$day $hour"}++;
      $inn_flow_total++;
      
      # Memorize the size. This can only be done with INN >= 1.5xx and
      # DO_LOG_SIZE = DO.
      
      # server <msg-id> size [feeds]
      # or
      # server <msg-id> (filename) size [feeds]
      
      my ($s) = $left =~ /^\S+ \S+ (?:\(\S+\) )?(\d+)(?: |$)/o;
      if ($s)
      {
	$inn_flow_size{"$day $hour"} += $s;
	$inn_flow_size_total += $s;
      }
      return 1;
    }

    # 437 Duplicate article
    if ($left =~ /(\S+) <[^>]+> 437 Duplicate(?: article)?$/o)
    {
      my $server = $1;
      $server =~ tr/A-Z/a-z/ unless ($CASE_SENSITIVE);
      $inn_badart{$server}++;
      $inn_duplicate{$server}++;
      return 1;
    }
    # 437 Unapproved for
    if ($left =~ /(\S+) <[^>]+> 437 Unapproved for \"([^\"]+)\"$/o)
    {
      my ($server, $group) = ($1, $2);
      $server =~ tr/A-Z/a-z/ unless ($CASE_SENSITIVE);
      $inn_badart{$server}++;
      $inn_unapproved{$server}++;
      $inn_unapproved_g{$group}++;
      return 1;
    }
    # 437 Too old -- ...
    if ($left =~ /(\S+) <[^>]+> 437 Too old -- /o)
    {
      my $server = $1;
      $server =~ tr/A-Z/a-z/ unless ($CASE_SENSITIVE);
      $inn_badart{$server}++;
      $inn_tooold{$server}++;
      return 1;
    }
    # 437 Unwanted site ... in path
    if ($left =~ /(\S+) <[^>]+> 437 Unwanted site (\S+) in path$/o)
    {
      my ($server, $site) = ($1, $2);
      $server =~ tr/A-Z/a-z/ unless ($CASE_SENSITIVE);
      $inn_badart{$server}++;
      $inn_uw_site{$server}++;
      $inn_site_path{$site}++;
      return 1;
    }
    # 437 Unwanted newsgroup "..."
    if ($left =~ /(\S+) <[^>]+> 437 Unwanted newsgroup \"(\S+)\"$/o)
    {
      my ($server, $group) = ($1, $2);
      $server =~ tr/A-Z/a-z/ unless ($CASE_SENSITIVE);
      $inn_badart{$server}++;
      $inn_uw_ng_s{$server}++;
      $inn_uw_ng{$group}++;
      return 1;
    }
    # 437 Unwanted distribution "..."
    if ($left =~ /(\S+) <[^>]+> 437 Unwanted distribution \"(\S+)\"$/o)
    {
      my ($server, $dist) = ($1, $2);
      $server =~ tr/A-Z/a-z/ unless ($CASE_SENSITIVE);
      $inn_badart{$server}++;
      $inn_uw_dist_s{$server}++;
      $inn_uw_dist{$dist}++;
      return 1;
    }
    # 437 Linecount x != y +- z
    if ($left =~ /(\S+) <[^>]+> 437 Linecount/o)
    {
      my $server = $1;
      $server =~ tr/A-Z/a-z/ unless ($CASE_SENSITIVE);
      $inn_badart{$server}++;
      $inn_linecount{$server}++;
      return 1;
    }
    # 437 No colon-space in "xxxx" header
    if ($left =~ /(\S+) <[^>]+> 437 No colon-space in \"[^\"]+\" header/o)
    {
      my $server = $1;
      $server =~ tr/A-Z/a-z/ unless ($CASE_SENSITIVE);
      $inn_badart{$server}++;
      $innd_others{$server}++;
      $innd_no_colon_space{$server}++;
      return 1;
    }
    # 437 Article posted in the future -- "xxxxx"
    if ($left =~ /(\S+) <[^>]+> 437 Article posted in the future -- \"[^\"]+\"/o)
    {
      my $server = $1;
      $server =~ tr/A-Z/a-z/ unless ($CASE_SENSITIVE);
      $innd_posted_future{$server}++;
      $innd_others{$server}++;
      $inn_badart{$server}++;
      return 1;
    }
    # all others are just counted as "Other"
    if ($left =~ /(\S+) /o)
    {
      my $server = $1;
      $server =~ tr/A-Z/a-z/ unless ($CASE_SENSITIVE);
      $inn_badart{$server}++;
      $innd_others{$server}++;
      return 1;
    }
  }

  ########
  ## innd
  if ($prog eq "innd")
  {  
    ## Note for innd logs:
    ## there's a lot of entries detected but still not used
    ## (because of a lack of interest).

    # think it's a dotquad
    return 1 if $left =~ /^think it\'s a dotquad$/o;
    if ($left =~ /^SERVER /o)
    {
      # SERVER perl filtering enabled
      return 1 if $left =~ /^SERVER perl filtering enabled$/o;
      # SERVER perl filtering disabled
      return 1 if $left =~ /^SERVER perl filtering disabled$/o;
      # SERVER cancelled +id
      return 1 if $left =~ /^SERVER cancelled /o;
    }
    # rejecting[perl]
    if ($left =~ /^rejecting\[perl\] <[^>]+> \d+ (.*)/o)
    {
      $innd_filter_perl{$1}++;
      return 1;
    }
    # closed lost
    return 1 if $left =~ /^\S+ closed lost \d+/o;
    # control command (by letter)
    if ($left =~ /^(\w)$/o)
    {
      my $command = $1;
      my $cmd = $ctlinnd{$command};
      $cmd = $command unless ($cmd);
      return 1 if ($cmd eq 'flush'); # to avoid a double count
      $innd_control{"$cmd"}++;
      return 1;
    }
    # control command (letter + reason)
    if ($left =~ /^(\w):.*$/o)
    {
      my $command = $1;
      my $cmd = $ctlinnd{$command};
      return 1 if ($cmd eq 'flush'); # to avoid a double count
      $innd_control{"$cmd"}++;
      return 1;
    }
    # opened
    return 1 if $left =~ /\S+ opened \S+:\d+:file$/o;
    # buffered
    return 1 if $left =~ /\S+ buffered$/o;
    # spawned
    return 1 if $left =~ /\S+ spawned \S+:\d+:proc:\d+$/o;
    return 1 if $left =~ /\S+ spawned \S+:\d+:file$/o;
    # running
    return 1 if $left =~ /\S+ running$/o;
    # sleeping
    if ($left =~ /(\S+):\d+:\proc:\d+ sleeping$/o)
    {
      my $server = $1;
      $server =~ tr/A-Z/a-z/ unless ($CASE_SENSITIVE);
      $innd_blocked{$server}++;
      return 1;
    }
    # blocked sleeping
    if ($left =~ /(\S+):\d+:\proc:\d+ blocked sleeping/o)
    {
      my $server = $1;
      $server =~ tr/A-Z/a-z/ unless ($CASE_SENSITIVE);
      $innd_blocked{$server}++;
      return 1;
    }
    if ($left =~ /(\S+):\d+ blocked sleeping/o)
    {
      my $server = $1;
      $server =~ tr/A-Z/a-z/ unless ($CASE_SENSITIVE);
      $innd_blocked{$server}++;
      return 1;
    }
    # restarted
    return 1 if $left =~ m/\S+ restarted$/o;
    # starting
    return 1 if $left =~ m/\S+ starting$/o;
    # readclose
    return 1 if $left =~ m/\S+:\d+ readclose+$/o;
    # connected
    if ($left =~ /(\S+) connected \d+/o)
    {
      my $server = $1;
      $server =~ tr/A-Z/a-z/ unless ($CASE_SENSITIVE);
      $innd_connect{$server}++;
      return 1;
    }
    # closed (with times)
    if ($left =~ /(\S+):\d+ closed seconds (\d+) accepted (\d+) refused (\d+) rejected (\d+)$/o)
    {
      my ($server, $seconds, $accepted, $refused, $rejected) =
	($1, $2, $3, $4, $5);
      $server =~ tr/A-Z/a-z/ unless ($CASE_SENSITIVE);
      $innd_seconds{$server} += $seconds;
      $innd_accepted{$server} += $accepted;
      $innd_refused{$server} += $refused;
      $innd_rejected{$server} += $rejected;
      return 1;
    }
    # closed (without times (?))
    return 1 if $left =~ m/\S+ closed$/o;
    # checkpoint
    return 1 if $left =~ m/^\S+:\d+ checkpoint /o;
    # if ($left =~ /(\S+):\d+ checkpoint seconds (\d+) accepted (\d+) 
    #     refused (\d+) rejected (\d+)$/)
    # {
    #   # Skipped...
    #   my ($server, $seconds, $accepted, $refused, $rejected) =
    #      ($1, $2, $3, $4, $5);
    #   $innd_seconds{$server} += $seconds;
    #   $innd_accepted{$server} += $accepted;
    #   $innd_refused{$server} += $refused;
    #   $innd_rejected{$server} += $rejected;
    #   return 1;
    # }

    # flush
    if ($left =~ /(\S+) flush$/o)
    {
      $innd_control{"flush"}++;
      return 1;
    }
    # flush-file
    if ($left =~ /flush_file/)
    {
       $innd_control{"flush_file"}++;
       return 1;
     }
    # overview exit 0 elapsed 23 pid 28461
    return 1 if $left =~ m/\S+ exit \d+ .*$/o;
    # internal rejecting huge article
    if ($left =~ /(\S+) internal rejecting huge article/o)
    {
      my $server = $1;
      $server =~ s/:\d+$//o;
      $server =~ tr/A-Z/a-z/ unless ($CASE_SENSITIVE);
      $innd_huge{$server}++;
      return 1;
    }
    # internal closing free channel
    if ($left =~ /(\S+) internal closing free channel/o)
    {
      $innd_misc{"Free channel"}++;
      return 1;
    }
    # internal (other)
    return 1 if $left =~ /\S+ internal/o;
    # wakeup
    return 1 if $left =~ /\S+ wakeup$/o;
    # throttle
    if ($left =~ /(\S+) throttled? /)
    {
      $innd_control{"throttle"}++;
      return 1;
    }
    # time (from the Greco's patch)
    # ME time X idle X(X) artwrite X(X) artlink X(X) hiswrite X(X) hissync
    # X(X) sitesend X(X) artctrl X(X) artcncl X(X) hishave X(X) hisgrep X(X) 
    # perl X(X)
    if ($left =~ m/^\S+\s+                         # ME
	           time\ (\d+)\s+                  # time
	           idle\ (\d+)\((\d+)\)\s+         # idle
                   artwrite\ (\d+)\((\d+)\)\s+     # artwrite
                   artlink\ (\d+)\((\d+)\)\s+      # artlink
                   hiswrite\ (\d+)\((\d+)\)\s+     # hiswrite
                   hissync\ (\d+)\((\d+)\)\s+      # hissync
                   sitesend\ (\d+)\((\d+)\)\s+     # sitesend
                   artctrl\ (\d+)\((\d+)\)\s+      # artctrl
                   artcncl\ (\d+)\((\d+)\)\s+      # artcncl
                   hishave\ (\d+)\((\d+)\)\s+      # hishave
                   hisgrep\ (\d+)\((\d+)\)\s+      # hisgrep
		   perl\ (\d+)\((\d+)\)\s+	   # perl (optional)
	           \s*$/ox)
    {
      $innd_time_times += $1;
      $innd_time_time{'idle'} += $2;
      $innd_time_num{'idle'} += $3;
      $innd_time_min{'idle'} = $2 / ($3 || 1)
	if ($3) && ($innd_time_min{'idle'} > $2 / ($3 || 1));
      $innd_time_max{'idle'} = $2 / ($3 || 1)
	if ($3) && ($innd_time_max{'idle'} < $2 / ($3 || 1));

      $innd_time_time{'article write'} += $4;
      $innd_time_num{'article write'} += $5;
      $innd_time_min{'article write'} = $4 / ($5 || 1)
	if ($5) && ($innd_time_min{'article write'} > $4 / ($5 || 1));
      $innd_time_max{'article write'} = $4 / ($5 || 1)
	if ($5) && ($innd_time_max{'article write'} < $4 / ($5 || 1));

      $innd_time_time{'article link'} += $6;
      $innd_time_num{'article link'} += $7;
      $innd_time_min{'article link'} = $6 / ($7 || 1)
	if ($7) && ($innd_time_min{'article link'} > $6 / ($7 || 1));
      $innd_time_max{'article link'} = $6 / ($7 || 1)
	if ($7) && ($innd_time_max{'article link'} < $6 / ($7 || 1));

      $innd_time_time{'history write'} += $8;
      $innd_time_num{'history write'} += $9;
      $innd_time_min{'history write'} = $8 / ($9 || 1)
	if ($9) && ($innd_time_min{'history write'} > $8 / ($9 || 1));
      $innd_time_max{'history write'} = $8 / ($9 || 1)
	if ($9) && ($innd_time_max{'history write'} < $8 / ($9 || 1));

      $innd_time_time{'history sync'} += $10;
      $innd_time_num{'history sync'} += $11;
      $innd_time_min{'history sync'} = $10 / ($11 || 1)
	if ($11) && ($innd_time_min{'history sync'} > $10 / ($11 || 1));
      $innd_time_max{'history sync'} = $10 / ($11 || 1)
	if ($11) && ($innd_time_max{'history sync'} < $10 / ($11 || 1));

      $innd_time_time{'site send'} += $12;
      $innd_time_num{'site send'} += $13;
      $innd_time_min{'site send'} = $12 / ($13 || 1)
	if ($13) && ($innd_time_min{'site send'} > $12 / ($13 || 1));
      $innd_time_max{'site send'} = $12 / ($13 || 1)
	if ($13) && ($innd_time_max{'site send'} < $12 / ($13 || 1));

      $innd_time_time{'article control'} += $14;
      $innd_time_num{'article control'} += $15;
      $innd_time_min{'article control'} = $14 / ($15 || 1)
	if ($15) && ($innd_time_min{'article control'} > $14 / ($15 || 1));
      $innd_time_max{'article control'} = $14 / ($15 || 1)
	if ($15) && ($innd_time_max{'article control'} < $14 / ($15 || 1));

      $innd_time_time{'article cancel'} += $16;
      $innd_time_num{'article cancel'} += $17;
      $innd_time_min{'article cancel'} = $16 / ($17 || 1)
	if ($17) && ($innd_time_min{'article cancel'} > $16 / ($17 || 1));
      $innd_time_max{'article cancel'} = $16 / ($17 || 1)
	if ($17) && ($innd_time_max{'article cancel'} < $16 / ($17 || 1));

      $innd_time_time{'history lookup'} += $18;
      $innd_time_num{'history lookup'} += $19;
      $innd_time_min{'history lookup'} = $18 / ($19 || 1)
	if ($19) && ($innd_time_min{'history lookup'} > $18 / ($19 || 1));
      $innd_time_max{'history lookup'} = $18 / ($19 || 1)
	if ($19) && ($innd_time_max{'history lookup'} < $18 / ($19 || 1));

      $innd_time_time{'history grep'} += $20;
      $innd_time_num{'history grep'} += $21;
      $innd_time_min{'history grep'} = $20 / ($21 || 1)
	if ($21) && ($innd_time_min{'history grep'} > $20 / ($21 || 1));
      $innd_time_max{'history grep'} = $20 / ($21 || 1)
	if ($21) && ($innd_time_max{'history grep'} < $20 / ($21 || 1));

      $innd_time_time{'perl filter'} += $22;
      $innd_time_num{'perl filter'} += $23;
      $innd_time_min{'perl filter'} = $22 / ($23 || 1)
	if ($23) && ($innd_time_min{'perl filter'} > $22 / ($23 || 1));
      $innd_time_max{'perl filter'} = $22 / ($23 || 1)
	if ($23) && ($innd_time_max{'perl filter'} < $22 / ($23 || 1));
      return 1;
    }
    # ME time xx idle xx(xx)     [ bug ? a part of timer ?]
    return 1 if $left =~ m/^ME time \d+ idle \d+\(\d+\)\s*$/o;
    # ME HISstats x hitpos x hitneg x missed x dne
    #
    # from innd/his.c:
    # HIShitpos: the entry existed in the cache and in history.
    # HIShitneg: the entry existed in the cache but not in history.
    # HISmisses: the entry was not in the cache, but was in the history file.
    # HISdne:    the entry was not in cache or history.
    if ($left =~ m/^ME\ HISstats                  # ME HISstats
	           \ (\d+)\s+hitpos               # hitpos
	           \ (\d+)\s+hitneg               # hitneg
	           \ (\d+)\s+missed               # missed
                   \ (\d+)\s+dne                  # dne
	           $/ox)
    {
      $innd_his{'Positive hits'} += $1;
      $innd_his{'Negative hits'} += $2;
      $innd_his{'Cache misses'}  += $3;
      $innd_his{'Do not exist'}  += $4;
      return 1;
    }
    # bad_hosts (appears after a "cant gesthostbyname" from a feed)
    return 1 if $left =~ m/\S+ bad_hosts /o;
    # cant read 
    return 1 if $left =~ m/(\S+) cant read/o;
    # cant write
    return 1 if $left =~ m/(\S+) cant write/o;
    # cant flush
    return 1 if $left =~ m/\S+ cant flush/o;
    # spoolwake
    return 1 if $left =~ m/\S+ spoolwake$/o;
    # spooling
    return 1 if $left =~ m/\S+ spooling/o;
    # DEBUG
    return 1 if $left =~ m/^DEBUG /o;
    # NCmode
    return 1 if $left =~ m/\S+ NCmode /o;
    # outgoing
    return 1 if $left =~ m/\S+ outgoing/o;
    # inactive
    return 1 if $left =~ m/\S+ inactive/o;
    # timeout
    return 1 if $left =~ m/\S+ timeout/o;
    # lcsetup
    return 1 if $left =~ m/\S+ lcsetup/o;
    # rcsetup
    return 1 if $left =~ m/\S+ rcsetup/o;
    # flush_all
    return 1 if $left =~ m/\S+ flush_all/o;
    # buffered
    return 1 if $left =~ m/\S+ buffered$/o;
    # descriptors
    return 1 if $left =~ m/\S+ descriptors/o;
    # ccsetup
    return 1 if $left =~ m/\S+ ccsetup/o;
    # renumbering
    return 1 if $left =~ m/\S+ renumbering/o;
    # renumber
    return 1 if $left =~ m/\S+ renumber /o;

    # newgroup
    if ($left =~ m/\S+ newgroup (\S+) as (\S)/o)
    {
      $innd_newgroup{$1} = $2;
      return 1;
    }
    # rmgroup
    if ($left =~ m/\S+ rmgroup (\S+)$/o)
    {
      $innd_rmgroup{$1}++;
      return 1;
    }
    # change_group
    if ($left =~ m/\S+ change_group (\S+) to (\S)/o)
    {
      $innd_changegroup{$1} = $2;
      return 1;
    }
    # paused
    if ($left =~ m/(\S+) paused /o)
    {
      $innd_control{"paused"}++;
      return 1;
    }
    # throttled
    return 1 if $left =~ m/\S+ throttled/o;
    # reload
    if ($left =~ m/(\S+) reload/o)
    {
      $innd_control{"reload"}++;
      return 1;
    }
    # shutdown
    if ($left =~ m/(\S+) shutdown/o)
    {
      $innd_control{"shutdown"}++;
      return 1;
    }
    # SERVER servermode paused
    return 1 if ($left =~ /(\S+) servermode paused$/o);
    # SERVER servermode running
    return 1 if ($left =~ /(\S+) servermode running$/o);
    # SERVER flushlogs paused
    if ($left =~ /(\S+) flushlogs /)
    {
      $innd_control{"flushlogs"}++;
      return 1;
    }
    # think it's a dotquad
    return 1 if $left =~ /think it\'s a dotquad: /o;
    # bad_ihave
    if ($left =~ /(\S+) bad_ihave /)
    {
      my $server = $1;
      $server =~ s/:\d+$//o;
      $server =~ tr/A-Z/a-z/ unless ($CASE_SENSITIVE);
      $innd_bad_ihave{$server}++;
      return 1;
    }
    # bad_messageid
    if ($left =~ /(\S+) bad_messageid/o)
    {
      my $server = $1;
      $server =~ s/:\d+$//o;
      $server =~ tr/A-Z/a-z/ unless ($CASE_SENSITIVE);
      $innd_bad_msgid{$server}++;
      return 1;
    }
    # bad_sendme
    if ($left =~ /(\S+) bad_sendme /o)
    {
      my $server = $1;
      $server =~ s/:\d+$//o;
      $server =~ tr/A-Z/a-z/ unless ($CASE_SENSITIVE);
      $innd_bad_sendme{$server}++;
      return 1;
    }
    # bad_command
    if ($left =~ /(\S+) bad_command /o)
    {
      my $server = $1;
      $server =~ s/:\d+$//o;
      $server =~ tr/A-Z/a-z/ unless ($CASE_SENSITIVE);
      $innd_bad_command{$server}++;
      return 1;
    }
    # bad_newsgroup
    if ($left =~ /(\S+) bad_newsgroup /o)
    {
      my $server = $1;
      $server =~ s/:\d+$//o;
      $innd_bad_newsgroup{$server}++;
      $server =~ tr/A-Z/a-z/ unless ($CASE_SENSITIVE);
      return 1;
    }
    if ($left =~ m/ cant /o)
    {
      # cant select Bad file number
      if ($left =~ / cant select Bad file number/o)
      {
	$innd_misc{"Bad file number"}++;
	return 1;
      }
      # cant gethostbyname
      if ($left =~ / cant gethostbyname/o)
      {
	$innd_misc{"gethostbyname error"}++;
	return 1;
      }
      # cant accept RCreader
      if ($left =~ / cant accept RCreader /o)
      {
	$innd_misc{"RCreader"}++;
	return 1;
      }
      # cant sendto CCreader
      if ($left =~ / cant sendto CCreader /o)
      {
	$innd_misc{"CCreader"}++;
	return 1;
      }
      # cant (other) skipped - not particularly interesting
      return 1;
    }
    # bad_newsfeeds no feeding sites
    return 1 if $left =~ /\S+ bad_newsfeeds no feeding sites/o;
    # CNFS-sm: cycbuff rollover - possibly interesting
    return 1 if $left =~ /CNFS-sm: cycbuff \S+ rollover to cycle/o;
  }
  ########
  ## innfeed
  if ($prog eq "innfeed")
  {
    # connected
    if ($left =~ /(\S+):\d+ connected$/)
    {
      my $server = $1;
      $server =~ tr/A-Z/a-z/ unless ($CASE_SENSITIVE);
      $innfeed_connect{$server}++;
      return 1;
    }
    # closed periodic
    return 1 if $left =~ m/\S+:\d+ closed periodic$/o;
    # periodic close
    return 1 if $left =~ m/\S+:\d+ periodic close$/o;
    # final (child)
    if ($left =~ /(\S+):\d+ final seconds (\d+) offered (\d+) accepted (\d+) refused (\d+) rejected (\d+)/o)
    {
      my ($server, $seconds, $offered, $accepted, $refused, $rejected) =
	($1, $2, $3, $4, $5, $6);
      $server =~ tr/A-Z/a-z/ unless ($CASE_SENSITIVE);
      $t_innfeed_seconds{$server} += $seconds;
      $t_innfeed_offered{$server} += $offered;
      $t_innfeed_accepted{$server} += $accepted;
      $t_innfeed_refused{$server} += $refused;
      $t_innfeed_rejected{$server} += $rejected;
      return 1;
    }
    # final (real)
    if ($left =~ /(\S+) final seconds (\d+) offered (\d+) accepted (\d+) refused (\d+) rejected (\d+) missing (\d+) spooled (\d+)/o)
    {
      my ($server, $seconds, $offered, $accepted, $refused, $rejected,
	  $missing, $spooled) = ($1, $2, $3, $4, $5, $6, $7, $8);
      $server =~ tr/A-Z/a-z/ unless ($CASE_SENSITIVE);
      $innfeed_seconds{$server} += $seconds;
      $innfeed_offered{$server} += $offered;
      $innfeed_accepted{$server} += $accepted;
      $innfeed_refused{$server} += $refused;
      $innfeed_rejected{$server} += $rejected;
      $innfeed_missing{$server} += $missing;
      $innfeed_spooled{$server} += $spooled;
      $t_innfeed_seconds{$server} = 0;
      $t_innfeed_offered{$server} = 0;
      $t_innfeed_accepted{$server} = 0;
      $t_innfeed_refused{$server} = 0;
      $t_innfeed_rejected{$server} = 0;
      return 1;
    }
    # final (only seconds & spooled)
    if ($left =~ /(\S+) final seconds (\d+) spooled (\d+)/o)
    {
      my ($server, $seconds, $spooled) = ($1, $2, $3);
      $server =~ tr/A-Z/a-z/ unless ($CASE_SENSITIVE);
      $innfeed_seconds{$server} += $seconds;
      $innfeed_spooled{$server} += $spooled;
      return 1;
    }
    # checkpoint
    return 1 if $left =~ m/\S+ checkpoint seconds/o;
    # ME file xxxx shrunk from yyyy to zzz
    if ($left =~ /^ME file (.*)\.output shrunk from (\d+) to (\d+)$/)
    {
      my ($file, $s1, $s2) = ($1, $2, $3);
      $file =~ s|^.*/([^/]+)$|$1|; # keep only the server name
      $innfeed_shrunk{$file} += $s1 - $s2;
      return 1;
    }
    # xxx grabbing external tape file
    return 1 if $left =~ m/ grabbing external tape file/o;
    # hostChkCxns - maxConnections was
    return 1 if $left =~ m/hostChkCxns - maxConnections was /o;
    # cxnsleep
    return 1 if $left =~ m/\S+ cxnsleep .*$/o;
    # idle
    return 1 if $left =~ m/\S+ idle tearing down connection$/o;
    # remote
    return 1 if $left =~ m/\S+ remote .*$/o;
    # spooling
    return 1 if $left =~ m/\S+ spooling no active connections$/o;
    # ME articles total
    return 1 if $left =~ m/(?:SERVER|ME) articles total \d+ bytes \d+/o;
    # ME articles active
    return 1 if $left =~ m/(?:SERVER|ME) articles active \d+ bytes \d+/o;
    # connect : Connection refused
    return 1 if $left =~ m/connect : Connection refused/o;
    # connect : No route to host
    return 1 if $left =~ m/connect : No route to host/o;
    # connection vanishing
    return 1 if $left =~ m/connection vanishing/o;
    # can't resolve hostname
    return 1 if $left =~ m/can\'t resolve hostname/o;
    # new hand-prepared backlog file
    return 1 if $left =~ m/new hand-prepared backlog file/o;
    # flush re-connect failed
    return 1 if $left =~ m/flush re-connect failed/o;
    # internal QUIT while write pending
    return 1 if $left =~ m/internal QUIT while write pending/o;
    # ME source lost . Exiting
    return 1 if $left =~ m/(?:SERVER|ME) source lost . Exiting/o;
    # ME starting innfeed (+version & date)
    return 1 if $left =~ m/(?:SERVER|ME) starting innfeed/o;
    # ME finishing at (date)
    return 1 if $left =~ m/(?:SERVER|ME) finishing at /o;
    # mode no-CHECK entered
    return 1 if $left =~ m/mode no-CHECK entered/o;
    # mode no-CHECK exited
    return 1 if $left =~ m/mode no-CHECK exited/o;
    # closed
    return 1 if $left =~ m/^(\S+) closed$/o;
    # global (+ seconds offered accepted refused rejected missing)
    return 1 if $left =~ m/^(\S+) global/o;
    # idle connection still has articles
    return 1 if $left =~ m/^(\S+) idle connection still has articles$/o;
    # missing article for IHAVE-body
    return 1 if $left =~ m/^(\S+) missing article for IHAVE-body$/o;
    # cannot continue
    return 1 if $left =~ m/^cannot continue/o;
    if ($left =~ /^(?:SERVER|ME)/o)
    {
      # ME dropping articles into ...
      return 1 if $left = ~/ dropping articles into /o;
      # ME dropped ...
      return 1 if $left = ~/ dropped /o;
      # ME internal bad data in checkpoint file
      return 1 if $left =~ m/ internal bad data in checkpoint/o;
      # ME two filenames for same article
      return 1 if $left =~ m/ two filenames for same article/o;
      # ME unconfigured peer
      return 1 if $left =~ m/ unconfigured peer/o;
      # exceeding maximum article size
      return 1 if $left =~ m/ exceeding maximum article byte/o;
      # no space left on device errors
      return 1 if $left =~ m/ ioerr fclose/o;
      return 1 if $left =~ m/ lock failed for host/o;
      return 1 if $left =~ m/ lock file pid-write/o;
      return 1 if $left =~ m/ locked cannot setup peer/o;
      return 1 if $left =~ m/ received shutdown signal/o;
      # unconfigured peer
      return 1 if $left =~ m/ unconfigured peer/o;
      # ME lock
      return 1 if $left =~ m/ lock/o;
      # ME exception: getsockopt (0): Socket operation on non-socket
      return 1 if $left =~ m/ exception: getsockopt /o;
      # ME config aborting fopen (...) Permission denied
      return 1 if $left =~ m/ config aborting fopen /o;
      # ME cant chmod innfeed.pid....
      return 1 if $left =~ m/ cant chmod \S+\/innfeed.pid/o;
      return 1 if $left =~ m/ tape open failed /o;
      return 1 if $left =~ m/ oserr open checkpoint file:/o;
      # ME finishing (quickly) 
      return 1 if $left =~ m/\(quickly\) /o;
      # ME config: value of streaming is not a boolean
      return 1 if $left =~ m/config: value of \S+ is not/o;
    }
    # hostChkCxn - now: x.xx, prev: x.xx, abs: xx, curr: x
    return 1 if $left =~ m/ hostChkCxn - now/o;
    # loading path_to_config_file/innfeed.conf
    return 1 if $left =~ m/loading /o;
    # Finnaly, to avoid problems with strange error lines, ignore them.
    #return 1 if ($left =~ /ME /);
  }
  ########
  ## innxmit
  if ($prog eq "innxmit")
  {
    # 437 Duplicate article
    if ($left =~ /(\S+) rejected <[^>]+> \(.*?\) 437 Duplicate article$/o)
    {
      my $server = $1;
      $server =~ tr/A-Z/a-z/ unless ($CASE_SENSITIVE);
      $innxmit_badart{$server}++;
      $innxmit_duplicate{$server}++;
      return 1;
    }
    # 437 Unapproved for
    if ($left =~ /(\S+) rejected <[^>]+> \(.*\) 437 Unapproved for \"(.*?)\"$/o)
    {
      my ($server, $group) = ($1, $2);
      $server =~ tr/A-Z/a-z/ unless ($CASE_SENSITIVE);
      $innxmit_badart{$server}++;
      $innxmit_unapproved{$server}++;
      $innxmit_unapproved_g{$group}++;
      return 1;
    }
    # 437 Too old -- ...
    if ($left =~ /(\S+) rejected <[^>]+> \(.*\) 437 Too old -- \".*?\"$/o)
    {
      my $server = $1;
      $server =~ tr/A-Z/a-z/ unless ($CASE_SENSITIVE);
      $innxmit_badart{$server}++;
      $innxmit_tooold{$server}++;
      return 1;
    }
    # 437 Unwanted site ... in path
    if ($left =~ 
      /(\S+) rejected <[^>]+> \(.*?\) 437 Unwanted site (\S+) in path$/o)
    {
      my ($server, $site) = ($1, $2);
      $server =~ tr/A-Z/a-z/ unless ($CASE_SENSITIVE);
      $innxmit_badart{$server}++;
      $innxmit_uw_site{$server}++;
      # $innxmit_site_path{$site}++;
      return 1;
    }
    # 437 Unwanted newsgroup "..."
    if ($left =~ 
      /(\S+) rejected <[^>]+> \(.*?\) 437 Unwanted newsgroup \"(\S+)\"$/o)
    {
      my ($server, $group) = ($1, $2);
      $server =~ tr/A-Z/a-z/ unless ($CASE_SENSITIVE);
      $innxmit_badart{$server}++;
      $innxmit_uw_ng_s{$server}++;
      $innxmit_uw_ng{$group}++;
      return 1;
    }
    # 437 Unwanted distribution "..."
    if ($left =~ 
      /(\S+) rejected <[^>]+> \(.*?\) 437 Unwanted distribution \"(\S+)\"$/o)
    {
      my ($server, $dist) = ($1, $2);
      $server =~ tr/A-Z/a-z/ unless ($CASE_SENSITIVE);
      $innxmit_badart{$server}++;
      $innxmit_uw_dist_s{$server}++;
      $innxmit_uw_dist{$dist}++;
      return 1;
    }
    # xx rejected foo.bar/12345 (foo/bar/12345) 437 Unwanted distribution "..."
    if ($left =~ /^(\S+) rejected .* 437 Unwanted distribution \"(\S+)\"$/o)
    {
      my ($server, $dist) = ($1, $2);
      $server =~ tr/A-Z/a-z/ unless ($CASE_SENSITIVE);
      $innxmit_badart{$server}++;
      $innxmit_uw_dist_s{$server}++;
      $innxmit_uw_dist{$dist}++;
      return 1;
    }
    # 437 Linecount x != y +- z
    if ($left =~ /(\S+) rejected <[^>]+> \(.*?\) 437 Linecount/o)
    {
      my $server = $1;
      $server =~ tr/A-Z/a-z/ unless ($CASE_SENSITIVE);
      $innxmit_badart{$server}++;
      $innxmit_linecount{$server}++;
      return 1;
    }
    # 437 Newsgroup name illegal -- "xxx"
    if ($left =~ /(\S+) rejected .* 437 Newsgroup name illegal -- "[^\"]*"$/)
    {
      my $server = $1;
      $server =~ tr/A-Z/a-z/ unless ($CASE_SENSITIVE);
      $innxmit_others{$server}++;
      $innxmit_badart{$server}++;
      return 1;
    }
    # Streaming retries
    return 1 if ($left =~ /\d+ Streaming retries$/o);
    # ihave failed
    if ($left =~ /(\S+) ihave failed/o)
    {
      my $server = $1;
      $server =~ tr/A-Z/a-z/ unless ($CASE_SENSITIVE);
      $innxmit_ihfail{$server} = 1;
      if ($left = /436 \S+ NNTP \S+ out of space/o)
      {
	$innxmit_nospace{$server}++;
	return 1;
      }
      if ($left = /400 \S+ space/o)
      {
	$innxmit_nospace{$server}++;
	return 1;
      }
      if ($left = /400 Bad file/o)
      {
	$innxmit_crefused{$server}++;
	return 1;
      }
      if ($left = /480 Transfer permission denied/o)
      {
	$innxmit_crefused{$server}++;
	return 1;
      }
    }
    # stats
    if ($left =~
      /(\S+) stats offered (\d+) accepted (\d+) refused (\d+) rejected (\d+)$/o)
    {
      my ($server, $offered, $accepted, $refused, $rejected) =
	($1, $2, $3, $4, $5);
      $server =~ tr/A-Z/a-z/ unless ($CASE_SENSITIVE);
      $innxmit_offered{$server} += $offered;
      $innxmit_offered{$server} -= $innxmit_ihfail{$server}
        if ($innxmit_ihfail{$server});
      $innxmit_accepted{$server} += $accepted;
      $innxmit_refused{$server} += $refused;
      $innxmit_rejected{$server} += $rejected;
      $innxmit_site{$server}++;
      $innxmit_ihfail{$server} = 0;
      return 1;
    }
    # times
    if ($left =~ /(\S+) times user (\S+) system (\S+) elapsed (\S+)$/o)
    {
      my ($server, $user, $system, $elapsed) = ($1, $2, $3, $4);
      $server =~ tr/A-Z/a-z/ unless ($CASE_SENSITIVE);
      $innxmit_times{$server} += $elapsed;
      return 1;
    } 
    # connect & no space
    if ($left =~ /(\S+) connect \S+ 400 No space/o)
    {
      my $server = $1;
      $server =~ tr/A-Z/a-z/ unless ($CASE_SENSITIVE);
      $innxmit_nospace{$server}++;
      $innxmit_site{$server}++;
      return 1;
    }
    # connect & NNTP no space
    if ($left =~ /(\S+) connect \S+ 400 \S+ out of space/o)
    {
      my $server = $1;
      $server =~ tr/A-Z/a-z/ unless ($CASE_SENSITIVE);
      $innxmit_nospace{$server}++;
      $innxmit_site{$server}++;
      return 1;
    }
    # connect & loadav
    if ($left =~ /(\S+) connect \S+ 400 loadav/o)
    {
      my $server = $1;
      if ($left =~ /expir/i)
      {
	$server =~ tr/A-Z/a-z/ unless ($CASE_SENSITIVE);
	$innxmit_expire{$server}++;
	$innxmit_site{$server}++;
	return 1;
      }
    }
    # connect 400 (other)
    if ($left =~ /(\S+) connect \S+ 400/o)
    {
      my $server = $1;
      $server =~ tr/A-Z/a-z/ unless ($CASE_SENSITIVE);
      $innxmit_crefused{$server}++;
      $innxmit_site{$server}++;
      return 1;
    }
    # connect failed
    if ($left =~ /(\S+) connect failed/o)
    {
      my $server = $1;
      $server =~ tr/A-Z/a-z/ unless ($CASE_SENSITIVE);
      $innxmit_cfail_host{$server}++;
      $innxmit_site{$server}++;
      return 1;
    }
    # authenticate failed
    if ($left =~ /(\S+) authenticate failed/o)
    {
      my $server = $1;
      $server =~ tr/A-Z/a-z/ unless ($CASE_SENSITIVE);
      $innxmit_afail_host{$server}++;
      $innxmit_site{$server}++;
      return 1;
    }
    # xxx ihave failed 400 loadav [innwatch:hiload] yyy gt zzz
    if ($left =~ /^(\S+) ihave failed 400 loadav/o)
    {
      my $server = $1;
      $server =~ tr/A-Z/a-z/ unless ($CASE_SENSITIVE);
      $innxmit_hiload{$server}++;
      return 1;
    }
    # ihave failed
    return 1 if ($left =~ /\S+ ihave failed/o);
    # requeued (....) 436 No space
    return 1 if ($left =~ /\S+ requeued \S+ 436 No space/o);
    # requeued (....) 400 No space
    return 1 if ($left =~ /\S+ requeued \S+ 400 No space/o);
    # requeued (....) 436 Can't write history
    return 1 if ($left =~ /\S+ requeued \S+ 436 Can\'t write history/o);
    # unexpected response code
    return 1 if ($left =~ /unexpected response code /o);
  }

  ########
  ## nntplink
  if ($prog eq "nntplink")
  {
    $left =~ s/^(\S+):/$1/;
    # EOF
    if ($left =~ /(\S+) EOF /o)
    {
      my $server = $1;
      $server =~ tr/A-Z/a-z/ unless ($CASE_SENSITIVE);
      $nntplink_site{$server}++;
      $nntplink_eof{$server}++;
      return 1;
    }
    # Broken pipe
    if ($left =~ /(\S+) Broken pipe$/o)
    {
      my $server = $1;
      $server =~ tr/A-Z/a-z/ unless ($CASE_SENSITIVE);
      $nntplink_site{$server}++;
      $nntplink_bpipe{$server}++;
      return 1;
    }
    # already running - won't die
    return 1 if $left =~ /\S+ nntplink.* already running /o;
    # connection timed out
    if ($left =~ /(\S+) connection timed out/o)
    {
      my $server = $1;
      $server =~ tr/A-Z/a-z/ unless ($CASE_SENSITIVE);
      $nntplink_site{$server}++;
      $nntplink_bpipe{$server}++;
      return 1;
    }
    # greeted us with 400 No space
    if ($left =~ /(\S+) greeted us with 400 No space/o)
    {
      my $server = $1;
      $server =~ tr/A-Z/a-z/ unless ($CASE_SENSITIVE);
      $nntplink_site{$server}++;
      $nntplink_nospace{$server}++;
      return 1;
    }
    # greeted us with 400 loadav
    if ($left =~ /(\S+) greeted us with 400 loadav/o)
    {
      my $server = $1;
      $server =~ tr/A-Z/a-z/ unless ($CASE_SENSITIVE);
      $nntplink_site{$server}++;
      $nntplink_hiload{$server}++;
      return 1;
    }
    # greeted us with 400 (other)
    if ($left =~ /(\S+) greeted us with 400/o)
    {
      my $server = $1;
      $server =~ tr/A-Z/a-z/ unless ($CASE_SENSITIVE);
      $nntplink_site{$server}++;
      if ($left =~ /expir/i)
      {
	$nntplink_expire{$server}++;
      }
      else
      {
	$nntplink_fail{$server}++;
      }
      return 1;
    }
    # greeted us with 502
    if ($left =~ /(\S+) greeted us with 502/o)
    {
      my $server = $1;
      $server =~ tr/A-Z/a-z/ unless ($CASE_SENSITIVE);
      $nntplink_site{$server}++;
      $nntplink_auth{$server}++;
      return 1;
    }
    # sent authinfo
    if ($left =~ /(\S+) sent authinfo/o)
    {
      my $server = $1;
      $server =~ tr/A-Z/a-z/ unless ($CASE_SENSITIVE);
      $nntplink_site{$server}++;
      $nntplink_auth{$server}++;
      return 1;
    }
    # socket()
    if ($left =~ /(\S+) socket\(\): /o)
    {
      my $server = $1;
      $server =~ tr/A-Z/a-z/ unless ($CASE_SENSITIVE);
      $nntplink_site{$server}++;
      $nntplink_sockerr{$server}++;
      return 1;
    }
    # select()
    if ($left =~ /(\S+) select\(\) /o)
    {
      my $server = $1;
      $server =~ tr/A-Z/a-z/ unless ($CASE_SENSITIVE);
      $nntplink_site{$server}++;
      $nntplink_selecterr{$server}++;
      return 1;
    }
    # sent IHAVE
    if ($left =~ /(\S+) sent IHAVE/o)
    {
      my $server = $1;
      $server =~ tr/A-Z/a-z/ unless ($CASE_SENSITIVE);
      $nntplink_ihfail{$server}++;
      if (($left =~ / 436 /) && ($left =~ / out of space /))
      {
	$nntplink_fake_connects{$server}++;
	$nntplink_nospace{$server}++;
      }
      return 1;
    }
    # article .... failed(saved): 436 No space
    if ($left =~ /(\S+) .* failed\(saved\): 436 No space$/o)
    {
      my $server = $1;
      $server =~ tr/A-Z/a-z/ unless ($CASE_SENSITIVE);
      $nntplink_nospace{$server}++;
      return 1;
    }
    # article .. 400 No space left on device writing article file -- throttling
    if ($left =~ /(\S+) .* 400 No space left on device writing article file -- throttling$/o)
    {
      my $server = $1;
      $server =~ tr/A-Z/a-z/ unless ($CASE_SENSITIVE);
      $nntplink_nospace{$server}++;
      return 1;
    }
    # stats
    if ($left =~ /(\S+) stats (\d+) offered (\d+) accepted (\d+) rejected (\d+) failed (\d+) connects$/o)
    {
      my ($server, $offered, $accepted, $rejected, $failed, $connects) =
	($1, $2, $3, $4, $5, $6);
      $server =~ tr/A-Z/a-z/ unless ($CASE_SENSITIVE);
      $nntplink_offered{$server} += $offered - $nntplink_ihfail{$server}++;
      $nntplink_accepted{$server} += $accepted;
      $nntplink_rejected{$server} += $rejected;
      $nntplink_failed{$server} += $failed;
      $nntplink_connects{$server} += $connects;
      $nntplink_ihfail{$server} = 0;
      if ($nntplink_fake_connects{$server})
      {
	$nntplink_site{$server} += $nntplink_fake_connects{$server};
	$nntplink_fake_connects{$server} = 0;
      }
      else
      {
	$nntplink_site{$server}++;
      }
      return 1;
    }
    # xmit
    if ($left =~ /(\S+) xmit user (\S+) system (\S+) elapsed (\S+)$/o)
    {
      my ($server, $user, $system, $elapsed) = ($1, $2, $3, $4);
      $server =~ tr/A-Z/a-z/ unless ($CASE_SENSITIVE);
      $nntplink_times{$server} += $elapsed;
      return 1;
    }
    # xfer
    return 1 if $left =~ /\S+ xfer/o;
    # Links down .. x hours
    if ($left =~ /(\S+) Links* down \S+ \d+/o)
    {
      # Collected but not used
      # my $server = $1;
      # $server =~ tr/A-Z/a-z/ unless ($CASE_SENSITIVE);
      # $nntplink_down{$server} += $hours;
      return 1;
    }
    # 503 Timeout
    if ($left =~ /^(\S+) \S+ \S+ \S+ 503 Timeout/o)
    {
      # Collected but not used
      # my $server = $1;
      # $server =~ tr/A-Z/a-z/ unless ($CASE_SENSITIVE);
      # $nntplink_timeout{$server}++;
      return 1;
    }
    # read() error while reading reply
    if ($left =~ /^(\S+): read\(\) error while reading reply/o)
    {
      my $server = $1;
      $server =~ tr/A-Z/a-z/ unless ($CASE_SENSITIVE);
      $nntplink_failed{$server}++;
      return 1;
    }
    # Password file xxxx not found
    return 1 if $left =~ /^\S+ Password file \S+ not found/;
    # No such
    return 1 if $left =~ /^\S+ \S+ \S+ No such/;
    # already running
    return 1 if $left =~ /^\S+ \S+ already running/;
    # error reading version from datafile
    return 1 if $left =~ /error reading version from datafile/;
  }
  ########
  ## nnrpd
  if ($prog eq "nnrpd")
  { 
    # Fix a small bug of nnrpd (inn 1.4*)
    $left =~ s/^ /\? /o;
    # Another bug (in INN 1.5b1)
    return 1 if $left =~ /^\020\002m$/o; # ^P^Bm
    # bad_history at num for <ref>
    return 1 if $left =~ /bad_history at \d+ for /o;
    # timeout short
    return 1 if $left =~ /\S+ timeout short$/o;
    # < or > + (blablabla)
    return 1 if $left =~ /^\S+ [\<\>] /o;
    # cant opendir ... I/O error
    return 1 if $left =~ /\S+ cant opendir \S+ I\/O error$/o;
    # perl filtering enabled
    return 1 if $left =~ /perl filtering enabled$/o;
    # connect
    if ($left =~ /(\S+) connect$/o)
    {
      my $server = $1;
      $server =~ tr/A-Z/a-z/ unless ($CASE_SENSITIVE);
      $nnrpd_connect{$server}++;
      return 1;
    }      
    # group
    if ($left =~ /(\S+) group (\S+) (\d+)$/o)
    {
      my ($server, $group, $num) = ($1, $2, $3);
      $nnrpd_group{$group} += $num;
      my ($hierarchy) = $group =~ /^([^\.]+).*$/o;
      $nnrpd_hierarchy{$hierarchy} += $num;
      return 1;
    }
    # post failed 
    if ($left =~ /(\S+) post failed (.*)$/o)
    {
      my ($server, $error) = ($1, $2);
      $nnrpd_post_error{$error}++;
      return 1;
    }
    # post ok
    return 1 if $left =~ /\S+ post ok/o;
    # posts
    if ($left =~ /(\S+) posts received (\d+) rejected (\d+)$/o)
    {
      my ($server, $received, $rejected) = ($1, $2, $3);
      $server =~ tr/A-Z/a-z/ unless ($CASE_SENSITIVE);
      $nnrpd_post_ok{$server} += $received;
      $nnrpd_post_rej{$server} += $rejected;
      return 1;
    }
    # noperm post without permission
    if ($left =~ /(\S+) noperm post without permission/o)
    {
      my $server = $1;
      $server =~ tr/A-Z/a-z/ unless ($CASE_SENSITIVE);
      $nnrpd_post_rej{$server} ++;
      return 1;
    }
    # no_permission
    if ($left =~ /(\S+) no_(permission|access)$/o)
    {
      my $server = $1;
      $server =~ tr/A-Z/a-z/ unless ($CASE_SENSITIVE);
      $nnrpd_no_permission{$server}++;
      return 1;
    }
    # bad_auth
    if ($left =~ /(\S+) bad_auth$/o)
    {
      my $server = $1;
      $server =~ tr/A-Z/a-z/ unless ($CASE_SENSITIVE);
      $nnrpd_no_permission{$server}++;
      return 1;
    }
    # unrecognized + command
    if ($left =~ /(\S+) unrecognized (.*)$/o)
    {
      my ($server, $error) = ($1, $2);
      $server =~ tr/A-Z/a-z/ unless ($CASE_SENSITIVE);
      $error = "_null command_" if ($error !~ /\S/);
      $error =~ s/^(xmotd) .*$/$1/i if ($error =~ /^xmotd .*$/i);
      $nnrpd_unrecognized{$server}++;
      $nnrpd_unrecogn_cmd{$error}++;
      return 1;
    }
    # exit
    if ($left =~ /(\S+) exit articles (\d+) groups (\d+)$/o)
    {
      my ($server, $articles, $groups) = ($1, $2, $3);
      $server =~ tr/A-Z/a-z/ unless ($CASE_SENSITIVE);
      $nnrpd_connect{$server}++ if ($server eq '?');
      $nnrpd_groups{$server} += $groups;
      $nnrpd_articles{$server} += $articles;
      return 1;
    }
    # times
    if ($left =~ /(\S+) times user (\S+) system (\S+) elapsed (\S+)$/o)
    {
      my ($server, $user, $system, $elapsed) = ($1, $2, $3, $4);
      $server =~ tr/A-Z/a-z/ unless ($CASE_SENSITIVE);
      $nnrpd_times{$server} += $elapsed;
      return 1;
    }
    # timeout
    if ($left =~ /(\S+) timeout$/o)
    {
      my $server = $1;
      $server =~ tr/A-Z/a-z/ unless ($CASE_SENSITIVE);
      $nnrpd_timeout{$server}++;
      return 1;
    }
    # timeout in post
    if ($left =~ /(\S+) timeout in post$/o)
    {
      my $server = $1;
      $server =~ tr/A-Z/a-z/ unless ($CASE_SENSITIVE);
      $nnrpd_timeout{$server}++;
      return 1;
    }
    # cant read Connection timed out
    if ($left =~ /(\S+) cant read Connection timed out$/o)
    {
      my $server = $1;
      $server =~ tr/A-Z/a-z/ unless ($CASE_SENSITIVE);
      $nnrpd_timeout{$server}++;
      return 1;
    }
    # cant read Operation timed out
    if ($left =~ /(\S+) cant read Operation timed out$/o)
    {
      my $server = $1;
      $server =~ tr/A-Z/a-z/ unless ($CASE_SENSITIVE);
      $nnrpd_timeout{$server}++;
      return 1;
    }
    # cant read Connection reset by peer
    if ($left =~ /(\S+) cant read Connection reset by peer$/o)
    {
      my $server = $1;
      $server =~ tr/A-Z/a-z/ unless ($CASE_SENSITIVE);
      $nnrpd_reset_peer{$server}++;
      return 1;
    }
    # gethostbyaddr: xxx.yyy.zzz != a.b.c.d
    if ($left =~ /^gethostbyaddr: (.*)$/o)
    {
      my $msg = $1;
      $nnrpd_gethostbyaddr{$msg}++;
      return 1;
    }
    # cant gethostbyaddr
    if ($left =~ /\? cant gethostbyaddr (\S+) .*$/o)
    {
      my $ip = $1;
      $nnrpd_gethostbyaddr{$ip}++;
      return 1;
    }
    # cant getpeername
    if ($left =~ /\? cant getpeername/o)
    {
      # $nnrpd_getpeername++;
      $nnrpd_gethostbyaddr{"? (can't getpeername)"}++;
      return 1;
    }
    # ME dropping articles into ...
    return 1 if $left = ~/ME dropping articles into /o;
    # newnews (interesting but ignored till now)
    return 1 if $left =~ /^\S+ newnews /o;
    # cant fopen (ignored too)
    return 1 if $left =~ /^\S+ cant fopen /o;
    # cant read No route to host
    return 1 if $left =~ /cant read No route to host/o;
    # cant read Broken pipe
    return 1 if $left =~ /cant read Broken pipe/o;
    # ioctl: ...
    return 1 if $left =~ /^ioctl: /o;
  }
  ########
  ## inndstart
  if ($prog eq "inndstart")
  {
    # cant bind Address already in use
    # cant bind Permission denied
    return 1 if $left =~ /cant bind /o;
    # cant setgroups Operation not permitted
    return 1 if $left =~ /cant setgroups /o;
  }
  ########
  ## batcher
  if ($prog eq "batcher")
  {
    # times
    if ($left =~ /(\S+) times user (\S+) system (\S+) elapsed (\S+)$/o)
    {
      my ($server, $user, $system, $elapsed) = ($1, $2, $3, $4);
      $server =~ tr/A-Z/a-z/ unless ($CASE_SENSITIVE);
      # $batcher_user{$server} += $user;
      # $batcher_system{$server} += $system;
      $batcher_elapsed{$server} += $elapsed;
      return 1;
    }
    # stats
    if ($left =~ /(\S+) stats batches (\d+) articles (\d+) bytes (\d+)$/o)
    {
      my ($server, $batches, $articles, $bytes) = ($1, $2, $3, $4);
      $server =~ tr/A-Z/a-z/ unless ($CASE_SENSITIVE);
      $batcher_offered{$server} += $batches;
      $batcher_articles{$server} += $articles;
      $batcher_bytes{$server} += $bytes;
      return 1;
    }
  }
  ########
  ## rnews
  if ($prog eq "rnews")
  {
    # rejected connection
    if ($left =~ /rejected connection (.*)$/o)
    {
      $rnews_rejected{$1}++;
      return 1;
    }
    # cant open_remote
    if ($left =~ /(cant open_remote .*)$/o)
    {
      $rnews_rejected{$1}++;
      return 1;
    }
    # rejected 437 Unwanted newsgroup
    if ($left =~ /rejected 437 Unwanted newsgroup \"(.*)\"$/o)
    {
      $rnews_bogus_ng{$1}++;
      return 1;
    }
    # rejected 437 Unapproved for "xx"
    if ($left =~ /rejected 437 Unapproved for \"(.*)\"$/o)
    {
      $rnews_unapproved{$1}++;
      return 1;
    }
    # rejected 437 Unwanted distribution
    if ($left =~ /rejected 437 Unwanted distribution (.*)$/o)
    {
      $rnews_bogus_dist{$1}++;
      return 1;
    }
    # rejected 437 Bad "Date"
    if ($left =~ /rejected 437 Bad \"Date\" (.*)$/o)
    {
      $rnews_bogus_date{$1}++;
      return 1;
    }
    # rejected 437 Article posted in the future
    if ($left =~ /rejected 437 Article posted in the future -- \"(.*)\"$/o)
    {
      $rnews_bogus_date{"(future) $1"}++;
      return 1;
    }
    # rejected 437 Too old -- "..."
    if ($left =~ /rejected 437 Too old -- (.*)$/o)
    {
      $rnews_too_old++;
      return 1;
    }
    # rejected 437 Linecount...
    if ($left =~ /rejected 437 (Linecount) \d+ \!= \d+/o)
    {
      $rnews_linecount++;
      return 1;
    }
    # rejected 437 Duplicate article
    if ($left =~ /rejected 437 (Duplicate article)/o)
    {
      $rnews_duplicate++;
      return 1;
    }
    # duplicate <msg-id> path..
    if ($left =~ /^duplicate /o)
    {
      $rnews_duplicate++;
      return 1;
    }
    # offered <msg-id> feed
    if ($left =~ /^offered \S+ (\S+)/o)
    {
      my $host = $1;
      $host =~ tr/A-Z/a-z/ unless ($CASE_SENSITIVE);
      # Small hack used to join article spooled when innd is throttle.
      # In this situation, the hostname is a 8 hex digits string
      # To avoid confusions with real feeds, the first character is forced
      # to be a '3' or a '4' (will work between 9/7/1995 and 13/7/2012).
      $host = "Local postings" if $host =~ /^[34][0-9a-f]{7}$/;
      $rnews_host{$host}++;
      return 1;
    }
    # rejected 437 ECP rejected
    return 1 if $left =~ m/rejected 437 ECP rejected/o;
    # rejected 437 "Subject" header too long
    return 1 if $left =~ m/header too long/o;
    # bad_article missing Message-ID
    return 1 if $left =~ m/bad_article missing Message-ID/o;
    # cant unspool saving to xxx
    return 1 if $left =~ m/cant unspool saving to/o;
  }

  ###########
  ## ncmspool
  if ($prog eq "ncmspool")
  {
    # <article> good signature from foo@bar.com
    if ($left =~ /good signature from (.*)/o)
    {
      $nocem_goodsigs{$1}++;
      $nocem_totalgood++;
      $nocem_lastid = $1;
      return 1;
    }
    # <article> bad signature from foo@bar.com
    if ($left =~ /bad signature from (.*)/o)
    {
      $nocem_badsigs{$1}++;
      $nocem_goodsigs{$1} = 0 unless ($nocem_goodsigs{$1});
      $nocem_totalbad++;
      $nocem_lastid = $1;
      return 1;
    }
    # <article> contained 123 new 456 total ids
    if ($left =~ /contained (\d+) new (\d+) total ids/o)
    {
      $nocem_newids += $1;
      $nocem_newids{$nocem_lastid} += $1;
      $nocem_totalids += $2;
      $nocem_totalids{$nocem_lastid} += $2;
      return 1;
    }
    return 1;
  }

  ###########
  ## crosspost
  if ($prog eq "crosspost")
  {
    # seconds 1001 links 3182 0 symlinks 0 0 mkdirs 0 0
    # missing 13 toolong 0 other 0
    if ($left =~ /^seconds\ (\d+)
	           \ links\ (\d+)\ (\d+)
	           \ symlinks\ (\d+)\ (\d+)
	           \ mkdirs\ (\d+)\ (\d+)
	           \ missing\ (\d+)
	           \ toolong\ (\d+)
	           \ other\ (\d+)
	         $/ox)
    {
      $crosspost_time += $1;
      $crosspost{'Links made'} += $2;
      $crosspost{'Links failed'} += $3;
      $crosspost{'Symlinks made'} += $4;
      $crosspost{'Symlinks failed'} += $5;
      $crosspost{'Mkdirs made'} += $6;
      $crosspost{'Mkdirs failed'} += $7;
      $crosspost{'Files missing'} += $8;
      $crosspost{'Paths too long'} += $9;
      $crosspost{'Others'} += $10;
      return 1;
    }
  }

  ###########
  ## cnfsstat
  if ($prog eq "cnfsstat")
  {
    # Class ALT for groups matching "alt.*" article size min/max: 0/1048576
    # Buffer T3, len: 1953  Mbytes, used: 483.75 Mbytes (24.8%)   0 cycles
    if ($left =~ m|^Class\ (\S+)\ for\ groups\ matching\ \S+
                    (\ article\ size\ min/max:\ \d+/\d+)?
                    \ Buffer\ (\S+),
                    \ len:\ (\d+)\s+Mbytes,
                    \ used:\ ([\d.]+)\ Mbytes\ \([\d.]+%\)
                    \s+(\d+)\ cycles
                 $|ox)
    {
      my ($class, $buffer, $size, $used, $cycles) = ($1, $3, $4, $5, $6);
      my ($h, $m, $s) = $hour =~ m/^(\d+):(\d+):(\d+)$/;
      my $time = $h * 3600 + $m * 60 + $s;
      $size *= 1024 * 1024;
      $used *= 1024 * 1024;
      $cnfsstat{$buffer} = $class;

      # If the size changed, invalidate all of our running fill rate stats.
      if ($size != $cnfsstat_size{$buffer})
      {
        delete $cnfsstat_rate{$buffer};
        delete $cnfsstat_samples{$buffer};
        delete $cnfsstat_time{$buffer};
        $cnfsstat_size{$buffer} = $size;
      }
      elsif ($cnfsstat_time{$buffer})
      {
        # We want to gather the rate at which cycbuffs fill.  Store a
        # running total of bytes/second and a total number of samples.
        # Ideally we'd want a weighted average of those samples by the
        # length of the sample period, but we'll ignore that and assume
        # cnfsstat runs at a roughly consistent interval.
        my ($period, $added);
        $period = $time - $cnfsstat_time{$buffer};
        $period = 86400 - $cnfsstat_time{$buffer} + $time
          if ($period < 0);
        if ($cycles == $cnfsstat_cycles{$buffer})
        {
          $added = $used - $cnfsstat_used{$buffer};
        }
        elsif ($cycles > $cnfsstat_cycles{$buffer})
        {
          $added = $size * ($cycles - $cnfsstat_cycles{$buffer}) + $used;
        }
        if ($added > 0)
        {
          $cnfsstat_rate{$buffer} += $added / $period;
          $cnfsstat_samples{$buffer}++;
        }
      }
      $cnfsstat_used{$buffer} = $used;
      $cnfsstat_cycles{$buffer} = $cycles;
      $cnfsstat_time{$buffer} = $time;
      return 1;
    }
  }
  
  # Ignore following programs :
  return 1 if ($prog eq "uxfxn");
  return 1 if ($prog eq "beverage");
  return 1 if ($prog eq "newsx");
  return 1 if ($prog eq "demmf");
  return 1 if ($prog eq "nnnn");
  return 1 if ($prog eq "controlchan");
  return 0;
}

#################################
# Adjust some values..

sub adjust
{
  my ($first_date, $last_date) = @_;

  ## The following lines are commented to avoid the double
  ## count (at the flushlog and at the innfeed flush)
  ## => [2.1.8_8] uncommented because of a problem reported by
  ## matija.grabnar@arnes.si (no output of INNFEED data
  ## from innreport although the data in the logs is correctly
  ## parsed and understood).
  
  if (%t_innfeed_seconds && !(%innfeed_seconds))
  {
    # do it only of there are no data recorded but temporary
    my $server;
    foreach $server (sort keys (%t_innfeed_seconds))
    {
      $innfeed_seconds{$server} += $t_innfeed_seconds{$server};
      $innfeed_offered{$server} += $t_innfeed_offered{$server};
      $innfeed_accepted{$server} += $t_innfeed_accepted{$server}; 
      $innfeed_refused{$server} += $t_innfeed_refused{$server};
      $innfeed_rejected{$server} += $t_innfeed_rejected{$server};
    }
  }
  
  my $nnrpd_doit = 0;
  my $curious;
  
  {
    my $serv;
    if (%nnrpd_connect)
    {
      my $c = keys (%nnrpd_connect);
      foreach $serv (keys (%nnrpd_connect))
      {
	if ($nnrpd_no_permission{$serv})
	{
	  undef ($nnrpd_connect{$serv});
	  undef ($nnrpd_groups{$serv});
	  undef ($nnrpd_times{$serv});
	  $c--;
	}
	$nnrpd_doit++
	  if ($nnrpd_groups{$serv} || $nnrpd_post_ok{$serv});
      }
      undef %nnrpd_connect  unless $c;
    }
    foreach $serv (keys (%nnrpd_groups))
    {
      $curious = "ok"
	unless ($nnrpd_groups{$serv} || $nnrpd_post_ok{$serv} ||
		$nnrpd_articles{$serv});
    }
  }
  
  # Fill some hashes
  {
    my $key;
    foreach $key (keys (%innd_connect))
    {
      $innd_offered{$key} = ($innd_accepted{$key} || 0)
	+ ($innd_refused{$key} || 0)
	+ ($innd_rejected{$key} || 0);
    }
    
    
    # adjust min/max of innd timer stats.
    if (%innd_time_min)
    {
      foreach $key (keys (%innd_time_min))
      {
	$innd_time_min{$key} = 0 if ($innd_time_min{$key} == $MIN);
	$innd_time_max{$key} = 0 if ($innd_time_max{$key} == $MAX);
	
	#$innd_time_min{$key} /= 1000;
	#$innd_time_max{$key} /= 1000;
      }
    }
    
    # remove the innd timer stats if not used.
    unless ($innd_time_times)
    {
      undef %innd_time_min;
      undef %innd_time_max;
      undef %innd_time_num;
      undef %innd_time_time;
    }
    
    # adjust the crosspost stats.
    if (%crosspost)
    {
      foreach $key (keys (%crosspost))
      {
	$crosspost_times{$key} = $crosspost_time ? 
	  sprintf "%.2f", $crosspost{$key} / $crosspost_time * 60 : "?";
      }
    }
  }
  
  if (%inn_flow)
  {
    my ($prev_dd, $prev_d, $prev_h) = ("", -1, -1);
    my $day;
    foreach $day (sort datecmp keys (%inn_flow))
    {
      my ($r, $h) = $day =~ /^(.*) (\d+)$/;
      my $d = index ("JanFebMarAprMayJunJulAugSepOctNovDec",
		     substr ($r,0,3)) / 3 * 31 + substr ($r, 4, 2);
      $prev_h = $h if ($prev_h == -1);
      if ($prev_d == -1)
      {
	$prev_d = $d;
	$prev_dd = $r;
      }
      if ($r eq $prev_dd) # Same day and same month ?
      {
	if ($h != $prev_h)
	{
	  if ($h == $prev_h + 1)
	  {
	    $prev_h++;
	  }
	  else
	  {
	    my $j;
	    for ($j = $prev_h + 1; $j < $h; $j++)
	    {
	      my $t = sprintf "%02d", $j;
	      $inn_flow{"$r $t"} = 0;
	    }
	    $prev_h = $h;
	  }
	}
      }
      else
      {
	my $j;
	# then end of the first day...
	for ($j = ($prev_h == 23) ? 24 : $prev_h + 1; $j < 24; $j++)
	{
	  my $t = sprintf "%02d", $j;
	  $inn_flow{"$prev_dd $t"} = 0;
	}
	
	# all the days between (if any)
	# well, we can forget them as it is supposed to be a tool launched daily.
	
	# the beginning of the last day..
	for ($j = 0; $j < $h; $j++)
	{
	  my $t = sprintf "%02d", $j;
	  $inn_flow{"$r $t"} = 0;
	}
	$prev_dd = $r;
	$prev_d = $d;
	$prev_h = $h;
      }
    }
    my $first = 1;
    my (%hash, %hash_time, %hash_size, $date, $delay);
    foreach $day (sort datecmp keys (%inn_flow))
    {
      my ($r, $h) = $day =~ /^(.*) (\d+)$/o;
      if ($first)
      {
	$first = 0;
	my ($t) = $first_date =~ m/:(\d\d:\d\d)$/o;
	$date = "$day:$t - $h:59:59";
	$t =~ m/(\d\d):(\d\d)/o;
	$delay = 3600 - $1 * 60 - $2;
      }
      else
      {
	$date = "$day:00:00 - $h:59:59";
	$delay = 3600;
      }
      $hash{$date} = $inn_flow{$day};
      $hash_size{$date} = $inn_flow_size{$day};
      $inn_flow_labels{$date} = $h;
      $hash_time{$date} = $delay;
    }
    my ($h, $t) = $last_date =~ m/ (\d+):(\d\d:\d\d)$/o;
    my ($h2) = $date =~ m/ (\d+):\d\d:\d\d /o;
    my $date2 = $date;
    $date2 =~ s/$h2:59:59$/$h:$t/;
    $hash{$date2} = $hash{$date};
    undef $hash{"$date"};
    $hash_size{$date2} = $hash_size{$date};
    undef $hash_size{"$date"};
    $t =~ m/(\d\d):(\d\d)/o;
    $hash_time{$date2} = $hash_time{$date} - ($h2 == $h) * 3600 + $1 * 60 + $2;
    undef $hash_time{"$date"};
    $inn_flow_labels{$date2} = $h;
    %inn_flow = %hash;
    %inn_flow_time = %hash_time;
    %inn_flow_size = %hash_size;
  }
  
  if (%innd_bad_ihave)
  {
    my $key;
    my $msg = 'Bad ihave control messages received';
    foreach $key (keys %innd_bad_ihave)
    {
      $innd_misc_stat{$msg}{$key} = $innd_bad_ihave{$key};
    }
  }
  if (%innd_bad_msgid)
  {
    my $key;
    my $msg = 'Bad Message-ID\'s offered';
    foreach $key (keys %innd_bad_msgid)
    {
      $innd_misc_stat{$msg}{$key} = $innd_bad_msgid{$key};
    }
  }
  if (%innd_bad_sendme)
  {
    my $key;
    my $msg = 'Ignored sendme control messages received';
    foreach $key (keys %innd_bad_sendme)
    {
      $innd_misc_stat{$msg}{$key} = $innd_bad_sendme{$key};
    }
  }
  if (%innd_bad_command)
  {
    my $key;
    my $msg = 'Bad command received';
    foreach $key (keys %innd_bad_command)
    {
      $innd_misc_stat{$msg}{$key} = $innd_bad_command{$key};
    }
  }
  if (%innd_bad_newsgroup)
  {
    my $key;
    my $msg = 'Bad newsgroups received';
    foreach $key (keys %innd_bad_newsgroup)
    {
      $innd_misc_stat{$msg}{$key} = $innd_bad_newsgroup{$key};
    }
  }
  if (%innd_posted_future)
  {
    my $key;
    my $msg = 'Article posted in the future';
    foreach $key (keys %innd_posted_future)
    {
      $innd_misc_stat{$msg}{$key} = $innd_posted_future{$key};
    }
  }
  if (%innd_no_colon_space)
  {
    my $key;
    my $msg = 'No colon-space in header';
    foreach $key (keys %innd_no_colon_space)
    {
      $innd_misc_stat{$msg}{$key} = $innd_no_colon_space{$key};
    }
  }
  if (%innd_huge)
  {
    my $key;
    my $msg = 'Huge articles';
    foreach $key (keys %innd_huge)
    {
      $innd_misc_stat{$msg}{$key} = $innd_huge{$key};
    }
  }
  if (%innd_blocked)
  {
    my $key;
    my $msg = 'Blocked server feeds';
    foreach $key (keys %innd_blocked)
    {
      $innd_misc_stat{$msg}{$key} = $innd_blocked{$key};
    }
  }
  if (%rnews_bogus_ng)
  {
    my $key;
    my $msg = 'Unwanted newsgroups';
    foreach $key (keys %rnews_bogus_ng)
    {
      $rnews_misc{$msg}{$key} = $rnews_bogus_ng{$key};
    }
  }
  if (%rnews_bogus_dist)
  {
    my $key;
    my $msg = 'Unwanted distributions';
    foreach $key (keys %rnews_bogus_dist)
    {
      $rnews_misc{$msg}{$key} = $rnews_bogus_dist{$key};
    }
  }
  if (%rnews_unapproved)
  {
    my $key;
    my $msg = 'Articles unapproved';
    foreach $key (keys %rnews_unapproved)
    {
      $rnews_misc{$msg}{$key} = $rnews_unapproved{$key};
    }
  }
  if (%rnews_bogus_date)
  {
    my $key;
    my $msg = 'Bad Date';
    foreach $key (keys %rnews_bogus_date)
    {
      $rnews_misc{$msg}{$key} = $rnews_bogus_date{$key};
    }
  }
  
  $rnews_misc{'Too old'}{'--'} = $rnews_too_old if $rnews_too_old;
  $rnews_misc{'Bad linecount'}{'--'} = $rnews_linecount if $rnews_linecount;
  $rnews_misc{'Duplicate articles'}{'--'} = $rnews_duplicate if $rnews_duplicate;
  
  if (%nnrpd_groups)
  {
    my $key;
    foreach $key (keys (%nnrpd_connect))
    {
      unless ($nnrpd_groups{"$key"} || $nnrpd_post_ok{"$key"} ||
	      $nnrpd_articles{"$key"})
      {
	$nnrpd_curious{$key} = $nnrpd_connect{$key};
	undef $nnrpd_connect{$key};
      }
    }
  }
}

sub report_unwanted_ng
{
  my $file = shift;
  open (FILE, "$file") && do {
    while (<FILE>)
    {
      my ($c, $n) = $_ =~ m/^\s*(\d+)\s+(.*)$/;
      next unless defined $n;
      $n =~ s/^newsgroup //o; # for pre 1.8 logs
      $inn_uw_ng{$n} += $c;
    }
    close (FILE);
  };

  unlink ("${file}.old");
  rename ($file, "${file}.old");
  
  open (FILE, "> $file") && do {
    my $g;
    foreach $g (sort {$inn_uw_ng{$b} <=> $inn_uw_ng{$a}} (keys (%inn_uw_ng)))
    {
      printf FILE "%d %s\n", $inn_uw_ng{$g}, $g;
    }
    close (FILE);
    chmod(0660, "$file");
  };
  unlink ("${file}.old");
}

###########################################################################

# Compare 2 dates (+hour)
sub datecmp
{
  # ex: "May 12 06"   for May 12, 6:00am
  local($[) = 0;
  # The 2 dates are near. The range is less than a few days that's why we
  # can cheat to determine the order. It is only important if one date
  # is in January and the other in December.

  my($date1) = substr($a, 4, 2) * 24;
  my($date2) = substr($b, 4, 2) * 24;
  $date1 += index("JanFebMarAprMayJunJulAugSepOctNovDec",substr($a,0,3)) * 288;
  $date2 += index("JanFebMarAprMayJunJulAugSepOctNovDec",substr($b,0,3)) * 288;
  if ($date1 - $date2 > 300 * 24)
  {
    $date2 += 288 * 3 * 12;
  }
  elsif ($date2 - $date1 > 300 * 24)
  {
    $date1 += 288 * 3 * 12;
  }
  $date1 += substr($a, 7, 2);
  $date2 += substr($b, 7, 2);
  $date1 - $date2;
}

1;
