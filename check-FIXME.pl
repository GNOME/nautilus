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

# check-FIXME.pl: Search for FIXMEs in the sources and correlate them
# with bugs in the bug database.

use diagnostics;
use strict;

# default to all the files starting from the current directory
my %skip_files;
if (!@ARGV)
  {
    @ARGV = `find -name '*' -and ! \\( -name '*~' -or -name '#*' -or -name 'ChangeLog*' -or -name 'Entries' \\)`;
    %skip_files =
      (
       "./TODO" => 1,
       "./aclocal.m4" => 1,
       "./check-FIXME.pl" => 1,
       "./config.sub" => 1,
       "./libtool" => 1,
       "./ltconfig" => 1,
       "./ltmain.sh" => 1,
       "./macros/gnome-fileutils.m4" => 1,
       "./macros/gnome-objc-checks.m4" => 1,
       "./macros/gnome-vfs.m4" => 1,
       "./src/file-manager/desktop-canvas.c" => 1,
       "./src/file-manager/desktop-layout.c" => 1,
       "./src/file-manager/desktop-window.c" => 1,
      );
  }

# locate all of the target lines
my $no_bug_lines = "";
my %bug_lines;
foreach my $file (@ARGV)
  {
    chomp $file;
    next if $skip_files{$file};
    next unless -T $file;
    open(FILE, $file) || die "can't open $file";
    while (<FILE>)
      {
        next if !/FIXME/;
        if (/FIXME bugzilla.eazel.com (\d+)/)
          {
            $bug_lines{$1} .= "$file:$.:$_";
          }
        else
          {
            $no_bug_lines .= "$file:$.:$_";
          }
      }
    close(FILE);
  }

# list the ones without bug numbers
if ($no_bug_lines ne "")
  {
    my @no_bug_list = sort split /\n/, $no_bug_lines;
    print "\n", scalar(@no_bug_list), " FIXMEs don't have bug reports:\n\n";
    foreach my $line (@no_bug_list)
      {
        print $line, "\n";
      }
  }

# list the ones with bugs that are not open
sub numerically { $a <=> $b; }
foreach my $bug (sort numerically keys %bug_lines)
  {
    # Check and see if the bug is open.
    my $page = `wget -q -O - http://bugzilla.eazel.com/show_bug.cgi?id=$bug`;
    $page =~ tr/\n/ /;
    my $status = "unknown";
    $status = $1 if $page =~ m|Status:.*</TD>\s*<TD>([A-Z]+)</TD>|;
    next if $status eq "NEW" || $status eq "ASSIGNED" || $status eq "REOPENED";

    # This a bug that is not open, so report it.
    my @bug_line_list = sort split /\n/, $bug_lines{$bug};
    print "\nBug $bug is $status:\n\n";
    foreach my $line (@bug_line_list)
      {
        print $line, "\n";
      }
  }
