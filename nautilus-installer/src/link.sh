#! /bin/bash

GNOME=/gnome

pushd `pwd`
cd ../../components/services/install/lib
    make -f makefile.staticlib clean && \
    #make CFLAGS="-g -Werror" DEFINES="-DEAZEL_INSTALL_NO_CORBA -DEAZEL_INSTALL_SLIM -DEAZEL_INSTALL_PROTOCOL_USE_OLD_CGI" -f makefile.staticlib && \
    make CFLAGS="-g -Werror" DEFINES="-DEAZEL_INSTALL_NO_CORBA -DEAZEL_INSTALL_SLIM" -f makefile.staticlib && \
    #make CFLAGS="-O -Werror" DEFINES="-DEAZEL_INSTALL_NO_CORBA -DEAZEL_INSTALL_SLIM -DEAZEL_INSTALL_PROTOCOL_USE_OLD_CGI" -f makefile.staticlib && \
    cd ../../trilobite/libtrilobite && \
    make -f makefile.staticlib clean && \
    make CFLAGS="-g -Werror" DEFINES="-DTRILOBITE_SLIM" -f makefile.staticlib && \
popd && \

make clean && \
#make CFLAGS="-O -Werror -DNO_TEXT_BOX $*" LDFLAGS="-static" DEFINES="-DNAUTILUS_INSTALLER_RELEASE" && \
#gcc -static -O -Werror -o nautilus-installer main.o support.o callbacks.o installer.o proxy.o 	\
make CFLAGS="-g -Werror -DNO_TEXT_BOX $*" LDFLAGS="-static" DEFINES="-DNAUTILUS_INSTALLER_RELEASE" && \
gcc -static -g -Werror -o nautilus-installer main.o support.o callbacks.o installer.o proxy.o 	\
../../components/services/install/lib/libeazelinstall_minimal.a 			\
../../components/services/trilobite/libtrilobite/libtrilobite_minimal.a 		\
../../libnautilus-extensions/nautilus-druid.o						\
../../libnautilus-extensions/nautilus-druid-page-eazel.o				\
-L$GNOME/lib -lgnomecanvaspixbuf -lgdk_pixbuf 						\
-lgnomeui -lgnome -lart_lgpl 								\
-lgtk -lgdk -lgmodule -lglib -lgdk_imlib 						\
-L/usr/X11R6/lib -ldl -lXext -lX11 -lm -lSM -lICE 					\
/usr/lib/libesd.a /usr/lib/libaudiofile.a -lghttp 					\
-L/usr/lib -lrpm -lbz2 -lz -ldb1 -lpopt -lxml 

cp nautilus-installer nautilus-installer-prezip

#echo Stripping...
#strip nautilus-installer
echo Packing...
gzexe nautilus-installer

echo Patching...
chmod 644 nautilus-installer
mv nautilus-installer hest
extraskip=`expr 22 + \`wc -l prescript|awk '{printf $1"\n"}'\``
echo "#!/bin/sh" > nautilus-installer.sh
echo "skip=$extraskip" >> nautilus-installer.sh
cat prescript >> nautilus-installer.sh
tail +3 hest >> nautilus-installer.sh
rm hest
echo Done...
