# $Id$
# Copyright (c)1998 G.J. Andruk
# checkgroups.pl - The checkgroups control message.
#  Parameters: params sender reply-to token site action[=log] approved
sub control_checkgroups {
  my $artfh;
  my @params = split(/\s+/,shift);
  my $sender = shift;
  my $replyto = shift;
  my $token = shift;
  my $site = shift;
  my ($action, $logging) = split(/=/, shift);
  my $approved = shift;
  
  my $newsgrouppats = shift;
  
  my $pid = $$;
  my $tempfile = "$inn::tmpdir/checkgroups.$pid";
  
  my ($errmsg, $status, $nc, @component, @oldgroup, $locktry,
      $ngname, $ngdesc, $modcmd);
  
  if ($action eq "mail") {
    open(TEMPFILE, ">$tempfile");
    print TEMPFILE ("$sender posted the following checkgroups message:\n");
    
    $artfh = open_article($token);
    next if (!defined($artfh));
    *ARTICLE = $artfh;
    
  CGHEAD:
    while (<ARTICLE>) {
      last CGHEAD if /^$/;
      s/^~/~~/;
      print TEMPFILE $_;
    }
    print TEMPFILE ("\nIf you want to process it, feed the body\n",
		    "of the message to docheckgroups while logged\n",
		    "in as user ID \"$newsuser\":\n\n",
		    "$inn::newslib/docheckgroups '$newsgrouppats' <<zRbJ\n");
    while (<ARTICLE>) {
      s/^~/~~/;
      print TEMPFILE ($_)   if (! /^\s+$/);
    }
    print TEMPFILE ("zRbJ\n");
    close(ARTICLE);
    close(TEMPFILE);
    logger($tempfile, "mail", "checkgroups by $sender\n");
    unlink($tempfile);
  } elsif ($action eq "log") {
    if (!$logging) {
      logmsg ('notice', 'checkgroups by %s', $sender);
    } else {
      logger($token, $logging, "checkgroups by $sender");
    }
  } elsif ($action eq "doit") {
    # Do checkgroups.
    open(TEMPART,">$tempfile.art");
    
    $artfh = open_article($token);
    next if (!defined($artfh));
    *ARTICLE = $artfh;
    
  CGHEAD2:
    while (<ARTICLE>) {
      last CGHEAD2 if /^$/;
    }
    while (<ARTICLE>) {
      print TEMPART ($_) if (! /^\s+$/);
    }
    close ARTICLE;
    close TEMPART;
    
    open OLDIN, "<&STDIN";
    open OLDOUT, "<&STDOUT";
    open STDIN, "<$tempfile.art";
    open STDOUT, ">$tempfile";
    system ("$inn::newslib/docheckgroups", $newsgrouppats);
    open STDIN, "<&OLDIN";
    open STDOUT, "<&OLDOUT";
    if (!$logging) {
      $logging = "mail";
    }
    logger($tempfile, $logging, "checkgroups by $sender");
    unlink ($tempfile, "$tempfile.art");
  }
}