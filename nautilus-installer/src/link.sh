#! /bin/bash
make clean
make CFLAGS="-O -Werror" LDFLAGS="-static"
gcc -static -O -Werror -o nautilus-installer main.o support.o interface.o callbacks.o installer.o ../../components/services/trilobite/libtrilobite/helixcode-utils.o ../../components/services/trilobite/libtrilobite/trilobite-core-distribution.o ../../components/services/install/lib/libinstall.a -L/gnome/lib -lgnomeui -lgnome -lart_lgpl -lgdk_imlib -lgtk -lgdk -lgmodule -lglib -L/usr/X11R6/lib -ldl -lXext -lX11 -lm -lSM -lICE /usr/lib/libesd.a /usr/lib/libaudiofile.a -lghttp -L/usr/lib -lrpm -lz -ldb1 -lpopt -lxml

echo Stripping...
strip nautilus-installer
echo Packing...
gzexe nautilus-installer

echo Patching...
chmod 644 nautilus-installer
mv nautilus-installer hest
extraskip=`expr 22 + \`wc -l prescript|awk '{printf $1"\n"}'\``
echo "#!/bin/sh" > nautilus-installer
echo "skip=$extraskip" >> nautilus-installer
cat prescript >> nautilus-installer
tail +3 hest >> nautilus-installer
rm hest
echo Done...
