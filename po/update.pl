#!/usr/bin/perl -w

#  GNOME PO Update Utility.
#  (C) 2000 The Free Software Foundation
#
#  Author(s): Kenneth Christiansen
#
#  GNOME PO Update Utility can use the XML to POT Generator, ui-extract.pl
#  Please distribute it along with this scrips, aswell as desk.po and
#  README.tools.
#
#  Also remember to change $PACKAGE to reflect the package the script is
#  used within.


my $VERSION = "1.5beta3";
my $LANG    = $ARGV[0];
my $PACKAGE = "nautilus";
$| = 1;


if (! $LANG){
    print "update.pl:  missing file arguments\n";
    print "Try `update.pl --help' for more information.\n";
    exit;
}

if ($LANG=~/^-(.)*/){

    if ("$LANG" eq "--version" || "$LANG" eq "-V"){
        &Version;
    }
    elsif ($LANG eq "--help" || "$LANG" eq "-H"){
	&Help;
    }
    elsif ($LANG eq "--dist" || "$LANG" eq "-D"){
        &Merging;
    }
    elsif ($LANG eq "--pot" || "$LANG" eq "-P"){
	if (-e ".headerlock"){
	unlink(".headerlock");
   	&GeneratePot;
	}else{
        &GenHeaders;
	&GeneratePot;}
        exit;
    }
    elsif ($LANG eq "--headers" || "$LANG" eq "-S"){
        &GenHeaders;
        exit;
    }
    elsif ($LANG eq "--maintain" || "$LANG" eq "-M"){
        &Maintain;
    }
    else {
        &InvalidOption;
    }

} else {
   if(-s "$LANG.po"){
        &GenHeaders; 
	&GeneratePot;
	&Merging;
	&Status;
   }  
   else {
	&NotExisting;       
   }
}

sub Version{
    print "GNOME PO Updater $VERSION\n";
    print "Written by Kenneth Christiansen <kenneth\@gnome.org>, 2000.\n\n";
    print "Copyright (C) 2000 Free Software Foundation, Inc.\n";
    print "This is free software; see the source for copying conditions.  There is NO\n";
    print "warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\n";
    exit;
}

sub Help{
    print "Usage: ./update.pl [OPTIONS] ...LANGCODE\n";
    print "Updates pot files and merge them with the translations.\n\n";
    print "  -H, --help                   shows this help page\n";
    print "  -P, --pot                    generate the pot file only\n";
    print "  -S, --headers                generate the XML headerfiles in POTFILES.in\n";
    print "  -M, --maintain               search for missing files in POTFILES.in\n";
    print "  -V, --version                shows the version\n";
    print "\nExamples of use:\n";
    print "update.sh --pot    just creates a new pot file from the source\n";
    print "update.sh da       created new pot file and updated the da.po file\n\n";
    print "Report bugs to <kenneth\@gnome.org>.\n";
    exit;
}

sub Maintain{
    $a="find ../ -print | egrep '.*\\.(c|y|cc|c++|h|gob)' ";

    open(BUF2, "POTFILES.in") || die "update.pl:  there's no POTFILES.in!!!\n";
    
    print "Searching for missing _(\" \") entries...\n";
    
    open(BUF1, "$a|");

    @buf1_1 = <BUF1>;
    @buf1_2 = <BUF2>;

    if (-s ".potignore"){
        open FILE, ".potignore";
        while (<FILE>) {
            if ($_=~/^[^#]/o){
                push @bup, $_;
            }
        }
        print "Found .potignore: Ignoring files...\n";
        @buf1_2 = (@bup, @buf1_2);
    }

    foreach my $file (@buf1_1){
        open FILE, "<$file";
        while (<FILE>) {
            if ($_=~/_\(\"/o){
                $file = unpack("x3 A*",$file) . "\n";
                push @buf2_1, $file;
                last;
            }
        }
    }

    @buf3_1 = sort (@buf2_1);
    @buf3_2 = sort (@buf1_2);

    my %in2;
    foreach (@buf3_2) {
       $in2{$_} = 1;
    }

    foreach (@buf3_1){
       if (!exists($in2{$_})){
           push @result, $_ }
       }

    if(@result){
        open OUT, ">missing";
        print OUT @result;
        print "\nHere is the result:\n\n", @result, "\n";
        print "The file \"missing\" has been placed in the current directory.\n";
        print "Files supposed to be ignored should be placed in \".potignore\"\n";
    }
    else{
        print "\nWell, it's all perfect! Congratulation!\n";
    }         
}

sub InvalidOption{
    print "update.pl: invalid option -- $LANG\n";
    print "Try `update.pl --help' for more information.\n";
}
 
sub GenHeaders{

    if(-s "ui-extract.pl"){

    print "Found ui-extract.pl script\nRunning ui-extract...\n";

    open FILE, "<POTFILES.in";
    while (<FILE>) {
       if ($_=~ /(.*)(\.xml\.h)/o){
          $filename = "\.\./$1\.xml";
          $xmlfiles="\.\/ui-extract.pl --update $filename";
          system($xmlfiles);
          }
      
       elsif ($_=~ /(.*)(\.glade\.h)/o){
          $filename = "\.\./$1\.glade";
          $xmlfiles="\.\/ui-extract.pl --update $filename";
          system($xmlfiles);  
       }
    }
    close FILE;
    system("touch .headerlock");
}}


sub GeneratePot{

    print "Building the $PACKAGE.pot...\n";

    $GETTEXT ="xgettext --default-domain\=$PACKAGE --directory\=\.\."
             ." --add-comments --keyword\=\_ --keyword\=N\_"
             ." --files-from\=\.\/POTFILES\.in ";  
    $GTEST   ="test \! -f $PACKAGE\.po \|\| \( rm -f \.\/$PACKAGE\.pot "
             ."&& mv $PACKAGE\.po \.\/$PACKAGE\.pot \)";

    system($GETTEXT);
    system($GTEST);
    print "Wrote $PACKAGE.pot\n";

    if(-e ".headerlock"){
       unlink(".headerlock");

       print "Removing generated header (.h) files...";

       open FILE, "<POTFILES.in";
       while (<FILE>) {
          if ($_=~ /(.*)(\.xml\.h)/o){
             $filename = "\.\./$1\.xml.h";
	     unlink($filename);
          }
       }
       close FILE;
    }
    print "done\n";
}

sub Merging{

    if ($ARGV[1]){
        $LANG   = $ARGV[1]; 
    } else {
	$LANG   = $ARGV[0];
    }

    if ($ARGV[0] ne "--dist" && $ARGV[0] ne "-D") {
        print "Merging $LANG.po with $PACKAGE.pot...";
    }

    $MERGE="cp $LANG.po $LANG.po.old && msgmerge $LANG.po.old $PACKAGE.pot -o $LANG.po";

    system($MERGE);
    
    if ($ARGV[0] ne "--dist" && $ARGV[0] ne "-D") {
        print "\n\n";
    }

    unlink "messages";
    unlink "$LANG.po.old";
}

sub NotExisting{
    print "update.pl:  sorry, $LANG.po does not exist!\n";
    print "Try `update.pl --help' for more information.\n";    
    exit;
}

sub Status{
    $STATUS="msgfmt --statistics $LANG.po";
    
    system($STATUS);
    print "\n";   
}
