#! /usr/bin/perl
print "do a fixscript on me to put the appropriate require line in.\n"; exit;

# This script cleans the newsgroups file:
#   * Groups no longer in the active file are removed.
#   * Duplicate entries are removed.  The last of a set of duplicates
#     is the one retained.  That way, you could simply append the
#     new/revised entries from a docheckgroups run and then this script
#     will remove the old ones.
#   * Groups with no description are removed.
#   * Groups matching the $remove regexp are removed.

$remove='';
# $remove='^alt\.';

open ACT, $inn::active  or die "Can't open $inn::active: $!\n";
while(<ACT>) {
    ($group) = split;
    $act{$group} = 1 unless($remove ne "" && $group =~ /$remove/o);
}
close ACT;

open NG, $inn::newsgroups  or die "Can't open $inn::newsgroups: $!\n";
while(<NG>) {
    chomp;
    ($group, $desc) = split /\s+/,$_,2;
    next unless(defined $act{$group});

    next if(!defined $desc);
    next if($desc =~ /^[?\s]*$/);
    next if($desc =~ /^no desc(ription)?(\.)?$/i);

    $hist{$group} = $desc;
}
close NG;

open NG, ">$inn::newsgroups.new"  or die "Can't open $inn::newsgroups.new for write: $!\n";
foreach $group (sort keys %act) {
    if(defined $hist{$group}) {
	print NG "$group\t$hist{$group}\n" or die "Can't write: $!\n";
    }
}
close NG or die "Can't close: $!\n";

rename "$inn::newsgroups.new", $inn::newsgroups  or die "Can't rename $inn::newsgroups.new to $inn::newsgroups: $!\n";