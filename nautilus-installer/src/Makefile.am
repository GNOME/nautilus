## Process this file with automake to produce Makefile.in

NULL = 

OG_FLAG = -g -O2 -DDEBUG

WARN_FLAG = -Wall -Werror
#BUILD_DATE = `date +%d%b%y-%H%M`
BUILD_DATE = $(shell date +%d%b%y-%H%M)

if ENABLE_INSTALLER
INSTALLER_BINPROGRAMS = eazel-installer
INSTALLER_BINSCRIPTS = eazel-installer.sh
endif

if COMPILE_RPM_30
RPM_MAJOR = 3
endif
if COMPILE_RPM_40
RPM_MAJOR = 4
endif

EXTRA_DIST = create-eazel-installer.sh prescript

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

bin_SCRIPTS = $(INSTALLER_BINSCRIPTS)
bin_PROGRAMS = $(INSTALLER_BINPROGRAMS)

eazel_installer_SOURCES = 			\
	main.c 					\
	package-tree.c 				\
	gtk-hackery.c 				\
	callbacks.c callbacks.h 		\
	proxy.c proxy.h 			\
	installer.c installer.h 

CFLAGS = 					\
	$(OG_FLAG)				\
	$(WARN_FLAG) 				\
	-DBUILD_DATE=\"$(BUILD_DATE)\" 		\
	-DRPM_MAJOR=$(RPM_MAJOR)

eazel_installer_LDFLAGS = -all-static

eazel_installer_LDADD = 	\
	../install-lib/libeazelinstall_minimal.a			\
	../libtrilobite/libtrilobite_minimal.a		\
	$(top_builddir)/libnautilus-extensions/nautilus-druid.o						\
	$(top_builddir)/libnautilus-extensions/nautilus-druid-page-eazel.o				\
	$(RPM_LIBS) \
	$(GNOMECANVASPIXBUF_LIBS) \
	$(GNOMEUI_LIBS) \
	$(GDK_PIXBUF_LIBS) \
	$(GHTTP_LIBS) \
	$(XML_LIBS)

#       $(top_builddir)/components/services/trilobite/libtrilobite/trilobite-core-distribution.o 	\
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

eazel-installer.sh: eazel-installer prescript
	export CFLAGS="$(CFLAGS)" && sh create-eazel-installer.sh
