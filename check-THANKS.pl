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
                "Mike Engber" => "Michael Engber",
                "Pavel Cisler" => "Pavel Císler",
                "Pavel" => "Pavel Císler",
                "Robin Slomkowski" => "Robin * Slomkowski");

# Map from alternate email addresses of some users to canonical versions

my %email_map = ('at@ue-spacy.com' => 'tagoh@gnome.gr.jp',
                 'sopwith@eazel.com' => 'sopwith@redhat.com',
                 'chief_wanker@eazel.com' => 'eskil@eazel.com',
                 'eskil@eazel.om' => 'eskil@eazel.com',
                 'yakk@yakk.net' => 'yakk@yakk.net.au',
                 'linuxfan@ionet..net' => 'linuxfan@ionet.net',
                 'rslokow@eazel.com' => 'rslomkow@eazel.com',
                 'snickell@stanford.edu' => 'seth@eazel.com',
                 'mathieu@gnome.org' => 'mathieu@eazel.com',
                 'hp@pobox.com' => 'hp@redhat.com',
                 'kmaraas@online.no' => 'kmaraas@gnome.org',
                 'kmaraas@gnu.org' => 'kmaraas@gnome.org',
                 'raph@gimp.org' => 'raph@acm.org',
                 'baulig@suse.de' => 'martin@home-of-linux.org'
                 'linuxfan@ionet.net' => 'josh@eazel.com');


# Some ChangeLog lines that carry no credit (incorrect changes that
# had to be reverted, etc)

my %no_credit = ('2000-09-08  Daniel Egger  <egger@suse.de>' => 1,
                 '2000-09-06  Daniel Egger  <egger@suse.de>' => 1);


my @lines;
my @sort_lines;
my @changelog_people;
my @thanks_people;
my @uncredited;
my @double_credited;



open (CHANGELOGS,"cat `find . -name intl -prune -or -name 'ChangeLog*' -print`|");


LOOP: while (<CHANGELOGS>) {
    my $name;
    my $email;

    chomp;

    if (/@/) {
        if ($no_credit{$_}) {
            next LOOP;
        }

        if (/^[0-9][0-9][0-9][0-9]-[0-9][0-9]-[0-9][0-9]/) {
            # Normal style ChangeLog comment 
            s/^[0-9][0-9][0-9][0-9]-[0-9][0-9]-[0-9][0-9][ \t]*//;

        } elsif (/^(Mon|Tue|Wed|Thu|Fri|Sat|Sun).*[0-9][0-9][0-9][0-9]/) {
            # Old style ChangeLog comment 
            s/^.*2000[ \t\n\r]*//;

        } else { 
            # FIXME: we should try to extract names & addresses from 
            # entry body text.

            next LOOP; # ignore unknown lines for now
        }

        
        $name = $_;
        $email = $_;
        
        $name =~ s/[ \t]*<.*[\n\r]*$//;

        if ($name_map{$name}) {
            $name = $name_map{$name};
        };

        
        $email =~ s/^.*<//;
        $email =~ s/>.*$//;
        $email =~ s/[ \t\n\r]*$//;
        
        if ($email_map{$email}) {
            $email = $email_map{$email};
        };

        push @lines, "${name}  <${email}>";
    }
}

close (CHANGELOGS);

@sort_lines = sort @lines;

my $last_line = "";

foreach my $line (@sort_lines) {
    push @changelog_people, $line unless $line eq $last_line;
    $last_line = $line;
}


open (AUTHORS, "<AUTHORS");

my @authors;

while (<AUTHORS>) {
    chomp;

    push @authors, $_;
}

close (AUTHORS);

open (THANKS, "<THANKS");


while (<THANKS>) {
    chomp;
    
    s/ - .*$//;

    push @thanks_people, $_;
}

close (THANKS);


foreach my $person (@changelog_people) {
    if (! (grep {$_ eq $person} @thanks_people) &&
        ! (grep {$_ eq $person} @authors)) {
        push @uncredited, $person;
    }
}

if (@uncredited) {
    print "The following people are in the ChangeLog but not credited in THANKS or AUTHORS:\n\n";

    foreach my $person (@uncredited) {
        print "${person}\n";
    }
    print "\n";
}



foreach my $person (@authors) {
    if (grep {$_ eq $person} @thanks_people) {
        push @double_credited, $person;
    }
}


if (@double_credited) {
    print "The following people are listed in both THANKS and AUTHORS:\n\n";

    foreach my $person (@double_credited) {
        print "${person}\n";
    }
}

# FIXME: we should also make sure that AUTHORS matches the contents of
# the About dialog.

print "\n";

