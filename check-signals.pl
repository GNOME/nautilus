#!/usr/bin/perl -w
# -*- Mode: perl; indent-tabs-mode: nil -*-

#
#  Nautilus
#
#  Copyright (C) 2000 Eazel, Inc.
#
#  This script is free software; you can redistribute it and/or
#  modify it under the terms of the GNU General Public License as
#  published by the Free Software Foundation; either version 2 of the
#  License, or (at your option) any later version.
#
#  This script is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
#  General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this script; if not, write to the Free Software
#  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
#
#  Author: Darin Adler <darin@bentspoon.com>,
#

# check-signals.pl: Search for .c files where someone forgot to
# put a call to gtk_object_class_add_signals.

use diagnostics;
use strict;

# default to all the files starting from the current directory
if (!@ARGV)
  {
    @ARGV = `find . -name '*.c' -print`;
  }

# locate all of the target lines
my @missing_files;
FILE: foreach my $file (@ARGV)
  {
    my $has_signal_new;
    my $has_add_signals;
    chomp $file;
    open FILE, $file or die "can't open $file";
    while (<FILE>)
      {
        $has_signal_new = 1 if /gtk_signal_new/;
        $has_add_signals = 1 if /gtk_object_class_add_signals/;
      }
    close FILE;
    push @missing_files, $file if $has_signal_new && !$has_add_signals;
  }

if (@missing_files)
  {
    print "\n", scalar(@missing_files), " C files are missing a call to gtk_object_class_add_signals.\n\n";
    print join("\n", @missing_files), "\n";
  }

