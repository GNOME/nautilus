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
	-DEAZEL_INSTALL_NO_CORBA				\
	-DEAZEL_INSTALL_SLIM					\
	-DNO_TEXT_BOX						\
	$(NULL)

bin_PROGRAMS = eazel-installer

eazel_installer_SOURCES = \
	main.c \
	package-tree.c \
	gtk-hackery.c \
	callbacks.c callbacks.h \
	proxy.c proxy.h \
	installer.c installer.h 

eazel_installer_LDFLAGS = -all-static

eazel_installer_LDADD = 	\
        $(top_builddir)/components/services/trilobite/libtrilobite/trilobite-core-distribution.o 	\
	$(top_builddir)/components/services/install/lib/libeazelinstall_minimal.a			\
	$(top_builddir)/components/services/trilobite/libtrilobite/libtrilobite_minimal.a		\
	$(top_builddir)/libnautilus-extensions/nautilus-druid.o						\
	$(top_builddir)/libnautilus-extensions/nautilus-druid-page-eazel.o				\
	$(RPM_LIBS) \
	$(GNOMECANVASPIXBUF_LIBS) \
	$(GNOMEUI_LIBS) \
	$(GDK_PIXBUF_LIBS) \
	$(GHTTP_LIBS) \
	$(XML_LIBS)

#	$(GNOME_LIBS) \
#	$(GTK_LIBS) \
#	$(GLIB_LIBS) \
#	$(GDK_PIXBUF_LIBS) \
#	-lpopt \
#	-lart_lgpl \
#	-lgdk_imlib \
#	-lgmodule \

#
#	/usr/lib/libesd.a \
#	/usr/lib/libaudiofile.a \
#	/gnome-source/nautilus/components/services/install/lib/libinstall.a			\
#	$(GNOME_LIBDIR) 	\
#	$(GNOMEUI_LIBS) 	\
#	$(GHTTP_LIBS)		\
#	$(INTLLIBS)		\
#	$(XML_LIBS)		\
#	$(RPM_LIBS)		\
#	$(NULL)

