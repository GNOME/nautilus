#! /bin/bash

DEBUG="no"
FULL="yes"

GNOME=/gnome
BUILD_DATE=`date +%d%b%y-%H%M`
XFREE=`rpm -q --queryformat="%{VERSION}" XFree86`
RPM_VERSION=`rpm -q --queryformat="%{VERSION}" rpm`

if test "x$1" = "x--help"; then
    echo
    echo "--debug / --no-debug    build with debug symbols or not"
    echo "--full / --quick        build all static libs too (full) or not"
    echo
    exit 0
fi

if test "x$1" = "x--debug"; then
    DEBUG="yes"
    echo "* Debug mode."
    shift
fi
if test "x$1" = "x--no-debug"; then
    DEBUG="no"
    echo "* Optimized mode."
    shift
fi
if test "x$1" = "x--full"; then
    FULL="yes"
    echo "* Full build."
    shift
fi
if test "x$1" = "x--no-full"; then
    FULL="no"
    echo "* Quick build."
    shift
fi
if test "x$1" = "x--quick"; then
    FULL="no"
    echo "* Quick build."
    shift
fi

if test "$DEBUG" = "yes"; then
    OG_FLAG="-g"
    STRIP="no"
else
    OG_FLAG="-O"
    STRIP="yes"
fi

if test "x$RPM_VERSION" = "x"; then
    echo "* No rpm installed?  Installer can only be built on RedHat for now...  Bye."
    exit 0
fi
RPM_MAJOR=`echo $RPM_VERSION | sed -e 's/\([0-9]\).*/\1/'`;
if test "x$RPM_MAJOR" = "x3"; then
    echo "* RedHat 6.x build (RPM 3)"
    export PACKAGE_SYSTEM_OBJECT=eazel-package-system-rpm3.o
else
    if test "x$RPM_MAJOR" = "x4"; then
        echo "* RedHat 7.x build (RPM 4)"
        export PACKAGE_SYSTEM_OBJECT=eazel-package-system-rpm4.o
    else
        echo "* RPM version $RPM_VERSION not supported (only 3 or 4)."
        exit 0
    fi
fi


XLIBS="-L/usr/X11R6/lib -ldl -lXext -lX11 -lm -lSM -lICE "
GLOG="-DG_LOG_DOMAIN=\\\"Nautilus-Installer\\\""
WARN_FLAG="-Wall -Werror"

if test "x$XFREE" = "x"; then
    echo "* XFree86 not installed as rpm, I will check for libXext";
    if test ! -f /usr/X11R6/lib/libXext.a; then
	echo "* libXext not present, not linking against it....";
	XLIBS="-L/usr/X11R6/lib -ldl -lX11 -lm -lSM -lICE ";
    else
	echo "* libXext found";
    fi
else
    XFREE_MAJOR=`echo $XFREE|sed -e 's/\([0-9]\).[0-9].[0-9]/\1/'`;
    if test "x$XFREE_MAJOR" = "x3"; then
	echo "* XFree86 3.x.y";
    elif test "x$XFREE_MAJOR" = "x4"; then
	echo "* XFree86 4.x.y";
	XLIBS="-L/usr/X11R6/lib -ldl -lX11 -lm -lSM -lICE ";
    else 
       echo "* I do not believe your XFree86 is a $XFREE_MAJOR";
       return 1;
    fi
fi

if test "x$FULL" = "xyes"; then
    pushd `pwd`
    cd ../../components/services/install/lib
        make -f makefile.staticlib clean
        make CFLAGS="$OG_FLAG $WARN_FLAG $GLOG" DEFINES="-DEAZEL_INSTALL_NO_CORBA -DEAZEL_INSTALL_SLIM" -f makefile.staticlib && \
        cd ../../trilobite/libtrilobite && \
        make -f makefile.staticlib clean && \
        make CFLAGS="$OG_FLAG $WARN_FLAG $GLOG" DEFINES="-DTRILOBITE_SLIM" -f makefile.staticlib && \
    popd
    if test $? -ne 0; then
        echo "* Aborting."
        exit 1
    fi
fi

make clean && \
make CFLAGS="$OG_FLAG $WARN_FLAG -DNO_TEXT_BOX -DBUILD_DATE=\\\"${BUILD_DATE}\\\"" LDFLAGS="-static" && \
gcc -static $OG_FLAG $WARN_FLAG -o eazel-installer \
main.o callbacks.o installer.o proxy.o package-tree.o gtk-hackery.o			\
../../components/services/install/lib/libeazelinstall_minimal.a 			\
../../components/services/trilobite/libtrilobite/libtrilobite_minimal.a 		\
../../libnautilus-extensions/nautilus-druid.o						\
../../libnautilus-extensions/nautilus-druid-page-eazel.o				\
-L$GNOME/lib -lgnomecanvaspixbuf -lgdk_pixbuf 						\
-lgnomeui -lgnome -lart_lgpl 								\
-lgtk -lgdk -lgmodule -lglib -lgdk_imlib 						\
$XLIBS \
-lghttp											\
-L/usr/lib -lrpm -lbz2 -lz -ldb1 -lpopt -lxml

if test $? -ne 0; then
    echo "* Aborting."
    exit 1
fi

cp eazel-installer eazel-installer-prezip

if test "$STRIP" = "yes"; then
    echo "* Stripping..."
    strip eazel-installer
fi
echo "* Packing..."
gzexe eazel-installer

echo "* Patching..."
chmod 644 eazel-installer
mv eazel-installer hest
extraskip=`expr 22 + \`wc -l prescript|awk '{printf $1"\n"}'\``
echo "#!/bin/sh" > eazel-installer.sh
echo "skip=$extraskip" >> eazel-installer.sh
cat prescript >> eazel-installer.sh
tail +3 hest >> eazel-installer.sh
rm hest

if test "$1" = "push" -a $? = 0; then
    echo "Copying installer to /h/public/bin ..."
    if test "$USER" = "robey"; then
        cp eazel-installer.sh /h/public/bin/
	# make it so anyone can write a new one in
	chmod 777 /h/public/bin/eazel-installer.sh
    else
        echo "You are not Robey, therefore you are lame.  Enter your password."
	chmod 777 ./eazel-installer.sh
        scp ./eazel-installer.sh odin.eazel.com:/h/public/bin/
    fi
fi

if test "$1" = "push-test" -a $? = 0; then
    echo "Copying installer to /h/public/robey ..."
    cp eazel-installer.sh /h/public/robey/
fi

echo '* Done!'
