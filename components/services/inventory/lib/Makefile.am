NULL =

CPPFLAGS = \
	-DPREFIX=\"$(prefix)\" 					\
	-DG_LOG_DOMAIN=\"trilobite-eazel-inventory\"		\
	-DSTANDALONE						\
	$(NULL)

INCLUDES = \
	-I$(top_srcdir)						\
	-I$(top_builddir)					\
        -I$(top_srcdir)/components/services/trilobite		\
        -I/usr/include/rpm					\
	$(GNOME_CFLAGS)						\
	$(GCONF_CFLAGS)						\
	$(GHTTP_CFLAGS)						\
	$(NULL)

noinst_LIBRARIES = libinventory.a

libinventory_a_SOURCES =						\
	eazel-inventory-shared.h					\
	eazel-inventory-shared.c					\
	$(NULL)

