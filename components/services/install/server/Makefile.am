NULL =

CPPFLAGS = \
	-DPREFIX=\"$(prefix)\" \
	-DG_LOG_DOMAIN=\"trilobite-eazel-install-service\" \
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\" \
	$(NULL)

INCLUDES = \
	-I$(top_srcdir) 	\
	-I$(top_builddir) 	\
	-I$(top_srcdir)/components/services/install/lib \
	-I$(top_srcdir)/components/services/trilobite \
	-I$(top_builddir)/components/services/install/lib \
	-I$(top_builddir)/components/services/trilobite \
	-I/usr/include/rpm	\
	$(GHTTP_CFLAGS) 	\
	$(GCONF_CFLAGS) 	\
	$(GNOME_CFLAGS) 	\
	$(GNOME_INCLUDEDIR) 	\
	$(OAF_CFLAGS) 		\
	$(XML_CFLAGS) 		\
	$(NULL)

LDFLAGS = \
	$(OAF_LIBS) 	\
	$(GHTTP_LIBS) 	\
	$(NULL)



pixmapdir = $(datadir)/pixmaps
pixmap_DATA = trilobite-eazel-install-service.png

# CORBA magic

oafdir = $(datadir)/oaf
oaf_DATA = trilobite-eazel-install-service.oafinfo

# make the auto-generated files depend on a installstamp file.
# this is to ensure the individual files wait for each other before
# proceeding which is needed in parallel makes

## Binaries

bin_PROGRAMS = trilobite-eazel-install-service

trilobite_eazel_install_service_SOURCES =	\
	main.c					\
	$(NULL)

trilobite_eazel_install_service_LDADD =							\
	$(top_builddir)/components/services/install/lib/libeazelinstall.la 		\
	$(top_builddir)/components/services/trilobite/libtrilobite/libtrilobite.la	 \
	$(top_builddir)/components/services/trilobite/libtrilobite/libtrilobite-service.la \
	$(GNOMEUI_LIBS) \
	$(RPM_LIBS)
	$(BONOBO_LIBS) \
	$(NULL)


## Extra stuff

EXTRA_DIST = \
	$(oaf_DATA)		\
	$(pixmap_DATA) 		\
	$(NULL)
