# $Id$
# Copyright (c)1998 G.J. Andruk
# version.pl - The version control message.
#  Parameters: params sender reply-to token site action[=log] approved
sub control_version {
  my $artfh;
  
  my @params = split(/\s+/,shift);
  my $sender = shift;
  my $replyto = shift;
  my $token = shift;
  my $site = shift;
  my ($action, $logging) = split(/=/, shift);
  my $approved = shift;
  
  my $VERSION = $inn::version;
  $VERSION = "(unknown version)" if (not $VERSION);
  
  my $groupname = $params[0];
  
  my $pid = $$;
  my $tempfile = "$inn::tmpdir/version.$pid";
  
  my ($errmsg, $status, $nc, @component, @oldgroup, $locktry,
      $ngname, $ngdesc, $modcmd, $kid);
  
  if ($action eq "mail") {
    #    open(TEMPFILE, ">$tempfile");
    print TEMPFILE  ("$sender has requested information about your\n",
		     "news software version.\n\n",
		     "If this is acceptable, type:\n",
		     "  echo \"InterNetNews\ $VERSION\" | ",
		     "$inn::mailcmd -s \"version reply from ",
		     "$inn::pathhost\" $replyto\n\n",
		     "The control message follows:\n\n");
    
    $artfh = open_article($token);
    next if (!defined($artfh));
    *ARTICLE = $artfh;
    
    print TEMPFILE $_ while <ARTICLE>;  
    close(ARTICLE);
    close(TEMPFILE);
    logger($tempfile, "mail", "version $sender\n");
    unlink($tempfile);
  } elsif ($action eq "log") {
    if (!$logging) {
      logmsg ('notice', 'version %s', $sender);
    } else {
      logger($token, $logging, "version $sender");
    }
  } elsif ($action =~ /^(doit|doifarg)$/) {
    if (($action eq "doifarg") && ($params[0] ne $inn::pathhost)) {
      logmsg ('notice', 'skipped version %s', $sender);
    } else {
      $kid = open2 (\*R, \*MAIL, $inn::mailcmd, "-s",
	     "version reply from $inn::pathhost", $replyto);
      print MAIL "InterNetNews $VERSION\n";
      close R;
      close MAIL;
      waitpid($kid, 0);
      # Now, log what we did.
      if ($logging) {
	$errmsg = "version $sender to $replyto";
	logger($token, $logging, $errmsg);
      }
    }
  }
}