CC=gcc
EAZEL_CFLAGS=-I../components/services/install/lib
EAZEL_LIBS=../components/services/install/lib/libinstall.a
GNET_CFLAGS=`gnet-config --cflags`
GNET_LIBS=/gnome/lib/libgnet.a
GHTTP_CFLAGS=`ghttp-config --cflags`
GHTTP_LIBS=/gnome/lib/libghttp.a
RPM_CFLAGS=-I/usr/include/rpm
RPM_LIBS=/usr/lib/librpm.a
XML_CFLAGS=`xml-config --cflags`
XML_LIBS=/gnome/lib/libxml.a
SHAREDIR=/gnome/share/eazel

all: nautilus-installer

nautilus-installer: nautilus-installer.o helixcode-utils.o
	$(CC) -static -g -O2 -o nautilus-installer nautilus-installer.o helixcode-utils.o -L/gnome/lib $(EAZEL_LIBS) $(RPM_LIBS) $(GNET_LIBS) $(GHTTP_LIBS) $(XML_LIBS) `/gnome/bin/gnome-config --libs gnomeui` -lz

helixcode-utils.o: helixcode-utils.c
	$(CC) -Wall -g -c -o helixcode-utils.o helixcode-utils.c -I. -I.. `/gnome/bin/gnome-config --cflags gnomeui`

nautilus-installer.o: nautilus-installer.c
	$(CC) -Wall -g -c -o nautilus-installer.o nautilus-installer.c -I. -I.. $(EAZEL_CFLAGS) `/gnome/bin/gnome-config --cflags gnomeui`


clean:
	rm -f nautilus-installer *.o
