#! /bin/bash

DEBUG="no"

GNOME=/gnome
BUILD_DATE=`date +%d%b%y-%H%M`

if test "$DEBUG" = "yes"; then
    OG_FLAG="-g"
    STRIP="no"
else
    OG_FLAG="-O"
    STRIP="yes"
fi

WARN_FLAG="-Wall -Werror"

pushd `pwd`
cd ../../components/services/install/lib
    make -f makefile.staticlib clean
    make CFLAGS="$OG_FLAG $WARN_FLAG" DEFINES="-DEAZEL_INSTALL_NO_CORBA -DEAZEL_INSTALL_SLIM" -f makefile.staticlib && \
    cd ../../trilobite/libtrilobite && \
    make -f makefile.staticlib clean && \
    make CFLAGS="$OG_FLAG $WARN_FLAG" DEFINES="-DTRILOBITE_SLIM" -f makefile.staticlib && \
popd && \

make clean && \
make CFLAGS="$OG_FLAG $WARN_FLAG -DNO_TEXT_BOX -DBUILD_DATE=\\\"${BUILD_DATE}\\\"" LDFLAGS="-static" && \
gcc -static $OG_FLAG $WARN_FLAG -o eazel-installer main.o support.o callbacks.o installer.o proxy.o 	\
../../components/services/install/lib/libeazelinstall_minimal.a 			\
../../components/services/trilobite/libtrilobite/libtrilobite_minimal.a 		\
../../libnautilus-extensions/nautilus-druid.o						\
../../libnautilus-extensions/nautilus-druid-page-eazel.o				\
-L$GNOME/lib -lgnomecanvaspixbuf -lgdk_pixbuf 						\
-lgnomeui -lgnome -lart_lgpl 								\
-lgtk -lgdk -lgmodule -lglib -lgdk_imlib 						\
-L/usr/X11R6/lib -ldl -lXext -lX11 -lm -lSM -lICE 					\
-lghttp											\
-L/usr/lib -lrpm -lbz2 -lz -ldb1 -lpopt -lxml

cp eazel-installer eazel-installer-prezip

if test "$STRIP" = "yes"; then
    echo Stripping...
    strip eazel-installer
fi
echo Packing...
gzexe eazel-installer

echo Patching...
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

echo 'Done!'
