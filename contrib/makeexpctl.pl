#!/usr/local/bin/perl

# Create expire.ctl script based on recently read articles.  Argument gives
# scale factor to use to adjust expires.

require '/var/netnews/lib/innshellvars.pl';

$readfile="$inn'pathdb/readgroups";

$expirectl=$inn'expirectl;
if (open(RDF, $readfile)) {
    while (<RDF>) {
	chop;
	@foo=split(/ /); # foo[0] should be group, foo[1] lastreadtime
	if ($foo[1] < $oldtime) {
	    next; # skip entries that are too old.
	}
	$groups{$foo[0]} = $foo[1];
    }
    close(RDF);
}

$scale = $ARGV[0];
if ($scale <= 0) {
    die "invalid scale parameter\n";
}

rename($expirectl, "$expirectl.OLD") || die "rename $expirectl failed!\n";
open(OUTFILE, ">$expirectl") || die "open $expirectl for write failed!\n";

print OUTFILE <<'EOF' ;
##  expire.ctl - expire control file
##  Format:
##	/remember/:<keep>
##	<patterns>:<modflag>:<keep>:<default>:<purge>
##  First line gives history retention; other lines specify expiration
##  for newsgroups.  Must have a "*:A:..." line which is the default.
##	<patterns>	wildmat-style patterns for the newsgroups
##	<modflag>	Pick one of M U A -- modifies pattern to be only
##			moderated, unmoderated, or all groups
##	<keep>		Mininum number of days to keep article
##	<default>	Default number of days to keep the article
##	<purge>		Flush article after this many days
##  <keep>, <default>, and <purge> can be floating-point numbers or the
##  word "never."  Times are based on when received unless -p is used;
##  see expire.8

# How long to remember old history entries for.
/remember/:2
#
EOF

# defaults for most groups.
printline("*", "A", 1);
printline("alt*,misc*,news*,rec*,sci*,soc*,talk*,vmsnet*","U",3);
printline("alt*,misc*,news*,rec*,sci*,soc*,talk*,vmsnet*","M",5);
printline("comp*,gnu*,info*,ok*,ecn*,uok*", "U", 5);
printline("comp*,gnu*,info*,ok*,ecn*,uok*", "M", 7);
# and now handle each group that's regularly read, 
# assinging them 3* normal max expire
foreach $i (keys %groups) {
    printline($i, "A", 21);
}
# and now put some overrides for groups which are too likely to fill spool if
# we let them go to autoexpire. 
printline("*binaries*,*pictures*", "A", 0.5);
printline("control*","A",1);
printline("control.cancel","A",0.5);
printline("news.lists.filters,alt.nocem.misc","A",1);

close(OUTFILE);
exit(1);

sub printline {
    local($grpstr, $mflag, $len) = @_;
    print OUTFILE $grpstr,":",$mflag,":",$len*$scale,":",$len*$scale,":",$len*$scale,"\n";
}