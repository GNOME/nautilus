## Process this file with automake to produce Makefile.in

NULL = 

INCLUDES = \
	-I$(top_srcdir)/libnautilus-extensions		 	\
	-I$(top_srcdir)/components/services/install/lib 	\
	-I$(top_srcdir)/components/services/trilobite		\
	-I/usr/include/rpm					\
	$(GNOMEUI_CFLAGS)					\
	$(XML_CFLAGS)						\
	$(GHTTP_CFLAGS)						\
	$(GNOME_INCLUDEDIR)					\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\" 		\
	-DG_LOG_DOMAIN=\"Nautilus-Installer\"			\
	$(NULL)

CPPFLAGS = -DEAZEL_INSTALL_NO_CORBA -DEAZEL_INSTALL_SLIM -DNO_TEXT_BOX

CFLAGS = -g
##LDFLAGS = -static

bin_PROGRAMS = nautilus-installer

nautilus_installer_SOURCES = \
	main.c \
	support.c support.h \
	callbacks.c callbacks.h \
	proxy.c proxy.h \
	installer.c installer.h 

nautilus_installer_LDFLAGS = -static

nautilus_installer_LDADD = 	\
        $(top_builddir)/components/services/trilobite/libtrilobite/helixcode-utils.o 			\
        $(top_builddir)/components/services/trilobite/libtrilobite/trilobite-core-distribution.o 	\
	$(top_builddir)/components/services/install/lib/libeazelinstall_minimal.a			\
	$(top_builddir)/components/services/trilobite/libtrilobite/libtrilobite_minimal.a		\
	$(top_builddir)/libnautilus-extensions/nautilus-druid.o						\
	$(top_builddir)/libnautilus-extensions/nautilus-druid-page-eazel.o				\
	-L/gnome/lib \
	-lgdk_pixbuf	\
	-lgnomecanvaspixbuf \
	-lgnomeui \
	-lgnome \
	-lart_lgpl \
	-lgdk_imlib \
	-lgtk \
	-lgdk \
	-lgmodule \
	-lglib \
	-L/usr/X11R6/lib -ldl -lXext -lX11 -lm -lSM -lICE \
	-lghttp \
	-L/usr/lib -lrpm -lz -ldb1 \
	-lpopt \
	-lxml 

#	/usr/lib/libesd.a \
#	/usr/lib/libaudiofile.a \


#
#        /gnome-source/nautilus/components/services/trilobite/libtrilobite/helixcode-utils.o 	\
#	/gnome-source/nautilus/components/services/install/lib/libinstall.a			\
#	$(GNOME_LIBDIR) 	\
#	$(GNOMEUI_LIBS) 	\
#	$(GHTTP_LIBS)		\
#	$(INTLLIBS)		\
#	$(XML_LIBS)		\
#	$(RPM_LIBS)		\
#	$(NULL)

