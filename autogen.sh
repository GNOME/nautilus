#!/bin/sh
# Run this to generate all the initial makefiles, etc.

srcdir=`dirname $0`
test -z "$srcdir" && srcdir=.

ORIGDIR=`pwd`
cd $srcdir

PROJECT=nautilus
TEST_TYPE=-f
FILE=src/Makefile.am

DIE=0

(autoconf --version) < /dev/null > /dev/null 2>&1 || {
	echo
	echo "You must have autoconf installed to compile $PROJECT."
	echo "Download the appropriate package for your distribution,"
	echo "or get the source tarball at ftp://ftp.gnu.org/pub/gnu/"
	DIE=1
}

(libtool --version) < /dev/null > /dev/null 2>&1 || {
	echo
	echo "You must have libtool installed to compile $PROJECT."
	echo "Get ftp://alpha.gnu.org/gnu/libtool-1.0h.tar.gz"
	echo "(or a newer version if it is available)"
	DIE=1
}

(automake --version) < /dev/null > /dev/null 2>&1 || {
	echo
	echo "You must have automake installed to compile $PROJECT."
	echo "Get ftp://ftp.cygnus.com/pub/home/tromey/automake-1.2d.tar.gz"
	echo "(or a newer version if it is available)"
	DIE=1
}

(xml-i18n-toolize --version) < /dev/null > /dev/null 2>&1 || {
	echo
	echo "You must have xml-i18n-tools installed to compile $PROJECT."
}

if test "$DIE" -eq 1; then
	exit 1
fi

test $TEST_TYPE $FILE || {
	echo "You must run this script in the top-level $PROJECT directory"
	exit 1
}

if test -z "$*"; then
	echo "I am going to run ./configure with no arguments - if you wish "
        echo "to pass any to it, please specify them on the $0 command line."
fi

case $CC in
*lcc | *lcc\ *) am_opt=--include-deps;;
esac

echo "Running gettextize...  Ignore non-fatal messages."
# Hmm, we specify --force here, since otherwise things don't
# get added reliably, but we don't want to overwrite intl
# while making dist.
echo "no" | gettextize --copy --force

echo "Running xml-i18n-toolize"
xml-i18n-toolize --copy --force --automake

echo "Running libtoolize"
libtoolize --copy --force

if test -z "$GNOME_INTERFACE_VERSION"; then
	ACLOCAL_FLAGS="-I hack-macros $ACLOCAL_FLAGS"
fi

aclocal $ACLOCAL_FLAGS

# optionally feature autoheader
(autoheader --version)  < /dev/null > /dev/null 2>&1 && autoheader

automake -a $am_opt

autoconf

cd $ORIGDIR

if [ "`whoami`" = "sopwith" ]; then
	SOPWITH_FLAGS_HACK="--enable-fatal-warnings=no --enable-more-warnings=no"
fi

$srcdir/configure --enable-maintainer-mode "$@" $SOPWITH_FLAGS_HACK

rv=$?

if [ $rv -eq 0 ]
then
    echo
    echo "Now type 'make' to compile $PROJECT."
    exit 0
fi

echo
echo "There was a problem running $srcdir/configure for $PROJECT."
exit 1
