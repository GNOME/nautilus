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
#  Author: Maciej Stachowiak
#

# check-THANKS.pl: Checks for users mentioned in one of the ChangeLogs
# but not in THANKS or AUTHORS; ensure that THANKS and AUTHORS do not
# overlap.

use diagnostics;
use strict;

# Map from alternate names of some users to canonical versions

my %name_map = ("Darin as Andy" => "Darin Adler",
                "J. Shane Culpepper" => "J Shane Culpepper",
                "Shane Culpepper" => "J Shane Culpepper",
                "Michael K. Fleming" => "Mike Fleming",
                "Rebecka Schulman" => "Rebecca Schulman",
                "Michael Engber" => "Mike Engber",
                "Pavel Cisler" => "Pavel Císler",
                "Pavel" => "Pavel Císler",
                "Eskil Olsen" => "Eskil Heyn Olsen",
		"Szabolcs BAN" => "Szabolcs Ban",
                "Robin Slomkowski" => "Robin * Slomkowski");

# Map from alternate email addresses of some users to canonical versions

my %email_map = ('at@ue-spacy.com' => 'tagoh@gnome.gr.jp',
                 'sopwith@eazel.com' => 'sopwith@redhat.com',
                 'chief_wanker@eazel.com' => 'eskil@eazel.com',
                 'yakk@yakk.net' => 'ian@eazel.com',
                 'yakk@yakk.net.au' => 'ian@eazel.com',
                 'linuxfan@ionet..net' => 'josh@eazel.com',
                 'snickell@stanford.edu' => 'seth@eazel.com',
                 'mathieu@gnome.org' => 'mathieu@eazel.com',
                 'hp@pobox.com' => 'hp@redhat.com',
                 'kmaraas@online.no' => 'kmaraas@gnome.org',
                 'kmaraas@gnu.org' => 'kmaraas@gnome.org',
                 'raph@gimp.org' => 'raph@acm.org',
                 'baulig@suse.de' => 'martin@home-of-linux.org',
                 'carlos@gnome-db.org' => 'carlos@hispalinux.es',
                 'mawa@iname.com' => 'mawarkus@gnome.org',
                 'linuxfan@ionet.net' => 'josh@eazel.com');


# Some ChangeLog lines that carry no credit (incorrect changes that
# had to be reverted, etc)

my %no_credit = ('2000-09-08  Daniel Egger  <egger@suse.de>' => 1,
                 '2000-09-06  Daniel Egger  <egger@suse.de>' => 1);


open CHANGELOGS, "cat `find . -name intl -prune -or -name 'ChangeLog*' -and \! -name '*~' -print`|" or die;

my @lines;
LOOP: while (<CHANGELOGS>)
  {
    chomp;
    
    if (/@/)
      {
        if ($no_credit{$_})
          {
            next LOOP;
          }

        if (/^\d\d\d\d-\d\d-\d\d/)
          {
            # Normal style ChangeLog comment 
            s/^\d\d\d\d-\d\d-\d\d[ \t]*//;
          }
        elsif (/^(Mon|Tue|Wed|Thu|Fri|Sat|Sun).*\d\d\d\d/)
          {
            # Old style ChangeLog comment 
            s/^.*20\d\d\s*//;
          }
        else
          {
            # FIXME bugzilla.eazel.com 3452: we should also try to extract
            # names & addresses from entry body text.
            next LOOP; # ignore unknown lines for now
          }
        
        my $name = $_;
        
        $name =~ s/[ \t]*<.*[\n\r]*$//;
        
        $name = $name_map{$name} if $name_map{$name};
        
        my $email = $_;
        
        $email =~ s/^.*<//;
        $email =~ s/>.*$//;
        $email =~ s/[ \t\n\r]*$//;
	$email =~ s/helixcode/ximian/;
        
        $email = $email_map{$email} if $email_map{$email};

        push @lines, "${name}  <${email}>";
    }
}

close CHANGELOGS;

my @changelog_people;
my $last_line = "";
foreach my $line (sort @lines)
  {
    push @changelog_people, $line unless $line eq $last_line;
    $last_line = $line;
  }

open AUTHORS, "AUTHORS" or die;

my @authors;

while (<AUTHORS>) {
    chomp;
    push @authors, $_;
}

close AUTHORS;

open THANKS, "THANKS" or die;

my @thanks_people;

while (<THANKS>) {
    chomp;
    s/ - .*$//;
    push @thanks_people, $_;
}

close THANKS;

open ABOUT, "src/nautilus-window-menus.c" or die;

my $found_about_authors = 0;

while (<ABOUT>)
  {
    if (/const char \*authors/)
      {
        $found_about_authors = 1;
        last;
      }
  }

my @about_authors;

if ($found_about_authors)
  {
    my $i = 0;
    while (<ABOUT>)
      {
        last unless /^\s+\"(.*)\",\s*\n/;
        push @about_authors, $1;
      }
  }

close ABOUT;


my @uncredited;
foreach my $person (@changelog_people)
  {
    if (! (grep {$_ eq $person} @thanks_people) &&
        ! (grep {$_ eq $person} @authors))
      {
        push @uncredited, $person;
      }
  }

my @double_credited;
foreach my $person (@authors)
  {
    if (grep {$_ eq $person} @thanks_people)
      {
        push @double_credited, $person;
      }
  }

my @author_names;
foreach my $person (@authors)
  {
    $person =~ s/\s*<.*//;
    push @author_names, $person;
  }

my @not_in_about;
foreach my $person (@author_names)
  {
    push @not_in_about, $person unless grep {$_ eq $person} @about_authors;
  }

my @only_in_about;
foreach my $person (@about_authors)
  {
    push @only_in_about, $person unless grep {$_ eq $person} @author_names;
  }

my $printed = 0;

if (@uncredited)
  {
    print "\nThe following people are in the ChangeLog but not credited in THANKS or AUTHORS:\n\n";
    
    foreach my $person (@uncredited)
      {
        print "${person}\n";
      }
    
    $printed = 1;
  }

if (@double_credited)
  {
    print "\nThe following people are listed in both THANKS and AUTHORS:\n\n";
    
    foreach my $person (@double_credited)
      {
        print "${person}\n";
      }
    
    $printed = 1;
  }

if (!$found_about_authors)
  {
    print "\nDidn't find authors section in nautilus-window-menus.c\n";
    $printed = 1;
  }

if (@not_in_about)
  {
    print "\nThe following people are in AUTHORS but not the about screen:\n\n";
    
    foreach my $person (@not_in_about)
      {
        print "${person}\n";
      }
    
    $printed = 1;
  }

if (@only_in_about)
  {
    print "\nThe following people are in the about screen but not AUTHORS:\n\n";
    
    foreach my $person (@only_in_about)
      {
        print "${person}\n";
      }
    
    $printed = 1;
  }

print "\n" if $printed;
