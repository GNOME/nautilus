#!/usr/bin/perl -w 

#  An XML to PO Template file converter
#  (C) 2000 The Free Software Foundation
#
#  Authors: Kenneth Christiansen <kenneth@gnu.org>


use strict;
use Getopt::Long;

my $PACKAGE	= "nautilus";
my $HELP_ARG 	= "0";
my $VERSION_ARG = "0";
my $VERSION 	= "0.5";
my %string 	= ();

$| = 1;

GetOptions (
	    "help|h|?"   => \$HELP_ARG,
	    "version|v"  => \$VERSION_ARG,
	    ) or &Error2;

&SplitOnArgument;


#---------------------------------------------------
# Check for options. 
# This section will check for the different options.
#---------------------------------------------------

sub SplitOnArgument {

    if ($VERSION_ARG) {
	&Version;

    } elsif ($HELP_ARG) {
	&Help;   

    } elsif (@ARGV > 0) {
	&Xmlfiles;

    } else {
	&Xmlfiles;

    }  
}    

#-------------------
sub Version{
    print "The XML to POT Converter $VERSION\n";
    print "Written by Kenneth Christiansen, 2000.\n\n";
    print "Copyright (C) 2000 Free Software Foundation, Inc.\n";
    print "This is free software; see the source for copying conditions.  There is NO\n";
    print "warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\n";
    exit;
}

#-------------------  
sub Help{
    print "Usage: xml2pot [FILENAME] [OPTIONS] ...\n";
    print "Generates a pot file from an xml source.\n\nGraps all strings ";
    print "between <_translatable_node> and it's end tag,\nwhere tag are all allowed ";
    print "xml tags. Read the docs for more info.\n\n"; 
    print "  -V, --version                shows the version\n";
    print "  -H, --help                   shows this help page\n";
    print "\nReport bugs to <kenneth\@gnu.org>.\n";
    exit;
}

#------------------- 
sub Error2{
#   print "xml2pot: invalid option @ARGV\n";
    print "Try `xml2pot --help' for more information.\n";
    exit;
}

sub Xmlfiles {
    print "Working, please wait.";

   if (-s "$PACKAGE-xml.pot"){
	unlink "$PACKAGE-xml.pot";
   }

    open FILE, "XMLFILES.in";
    while (<FILE>) {
        chomp $_;
        &Convert ($_);
    }
    close FILE;

    open OUT, ">>$PACKAGE-xml.pot";
    &addMessages;
    close OUT;

    system "msghack --append $PACKAGE-source.pot $PACKAGE-xml.pot > $PACKAGE.pot";
    unlink "$PACKAGE-xml.pot";
    unlink "$PACKAGE-source.pot";
    print  "done.\n";
}

#-------------------
sub Convert($) {

    #-----------------
    # Reading the file
    #-----------------
    my $input; {
	local (*IN);
	local $/; #slurp mode
	open (IN, "< ../$_[0]") || die "can't open $_[0]: $!";
	$input = <IN>;
    }

    my $fileName 	   = $_[0];
    my $count_input         = $input;
    my $check2_input        = $input;

    my $findline = sub ($){
	# lame and inefficient, mostly slow
	print ".";
	my $n; my $i = 1;
	for ($count_input =~ /(.|\n|\r|\t)/gm) {
	    last if $i == $_[0];
	    $i++;
	    $n++ if $_ eq "\n";   
	    $n++ if $_ eq "\r";
	}
	return $n;
    };

    if (!-s "$PACKAGE.xml.pot"){
    	open OUT, ">$PACKAGE-xml.pot";

	print OUT "# SOME DESCRIPTIVE TITLE.\n";
	print OUT "# Copyright (C) YEAR Free Software Foundation, Inc.\n";
	print OUT "# FIRST AUTHOR <EMAIL\@ADDRESS>, YEAR.\n";
	print OUT "#\n";
	print OUT "#, fuzzy\n";
	print OUT "msgid \"\"\n";
	print OUT "msgstr \"\"\n";
	print OUT "\"Project-Id-Version: PACKAGE VERSION\\n\"\n";
	print OUT "\"POT-Creation-Date: YEAH-MO-DA HO:MI+ZONE\\n\"\n";
        print OUT "\"PO-Revision-Date: YEAR-MO-DA HO:MI+ZONE\\n\"\n";
        print OUT "\"Last-Translator: FULL NAME <EMAIL\@ADDRESS>\\n\"\n";
        print OUT "\"Language-Team: LANGUAGE <LL\@li.org>\\n\"\n";
        print OUT "\"MIME-Version: 1.0\\n\"\n";
        print OUT "\"Content-Type: text/plain; charset=CHARSET\\n\"\n";
        print OUT "\"Content-Transfer-Encoding: ENCODING\\n\"\n";
			}   
       	close OUT;

        while ($input =~ /_[a-zA-Z0-9_]+=\"([^\"]*)\"/sg) {
	    my $lineNo = &$findline(length $`);
	   	$string{$1} = [$lineNo,$fileName];
        }

	while ($check2_input =~ /<_[a-zA-Z0-9_]+>(..[^_]*)<\/_[a-zA-Z0-9_]+>/sg) {
            my $lineNo = &$findline(length $`);
		$string{$1} = [$lineNo,$fileName];
 	}
    }

sub addMessages{

    # # inputfile: lineno
    # msgid "the string"
    # msgstr ""
    #
    # with
    #
    # # inputfile: lineno
    # # inputfile: newlineno
    # msgid "the string"
    # msgstr ""

    foreach my $theMessage (sort keys %string) {
	my ($lineNo,$fileName) = @{ $string{$theMessage} };

    if ($theMessage =~ /\n/) {
	print OUT "\n";
	print OUT "#: $fileName:$lineNo\n";
	print OUT "msgid \"\"\n"; 

        for (split /\n/, $theMessage) {
	    $_ =~ s/^\s+//mg;
	    print OUT "\"$_\"\n";
	}

	} else {
		
	    print OUT "\n";
	    print OUT "#: $fileName:$lineNo\n";
	    print OUT "msgid \"$theMessage\"\n";

	}
	    
	print OUT "msgstr \"\"\n";
    }
}

