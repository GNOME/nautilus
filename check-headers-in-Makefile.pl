#!/usr/bin/perl -w
# -*- Mode: perl; indent-tabs-mode: nil -*-

#
#  Nautilus
#
#  Copyright (C) 2000 Eazel, Inc.
#
#  This library is free software; you can redistribute it and/or
#  modify it under the terms of the GNU General Public License as
#  published by the Free Software Foundation; either version 2 of the
#  License, or (at your option) any later version.
#
#  This library is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
#  General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this library; if not, write to the Free Software
#  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
#
#  Author: Darin Adler <darin@eazel.com>,
#

# check-config-h.pl: Search for .c files where someone forgot to
# put an include for <config.h> in.

use diagnostics;
use strict;

# default to all the files starting from the current directory
if (!@ARGV)
  {
    @ARGV = `find . -name 'Makefile\.am' -print`;
  }

foreach my $file (@ARGV)
  {
    chomp $file;
    my $directory = $file;
    $directory =~ s|/Makefile\.am||;

    open FILE, $file or die "can't open $file";
    my %headers;
    while (<FILE>)
      {
	while (s/([-_a-zA-Z0-9]+\.[ch])\W//)
	  {
	    $headers{$1} = $1;
	  }
      }
    close FILE;

    if ($directory eq ".")
      {
	$headers{"acconfig.h"} = "acconfig.h";
	$headers{"config.h"} = "config.h";
      }

    opendir DIRECTORY, $directory or die "can't open $directory";
    foreach my $header (grep /.*\.[ch]$/, readdir DIRECTORY)
      {
	if (defined $headers{$header})
	  {
	    delete $headers{$header};
	  }
	else
	  {
	    print "$directory/$header in directory but not Makefile.am\n";
	  }
      }
    closedir DIRECTORY;

    foreach my $header (keys %headers)
      {
	print "$directory/$header in Makefile.am but not directory\n";
      }
  }
