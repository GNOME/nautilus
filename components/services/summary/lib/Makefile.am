NULL =

CPPFLAGS = \
	-DPREFIX=\"$(prefix)\" 						\
	-DG_LOG_DOMAIN=\"trilobite-eazel-summary\"			\
	-DSTANDALONE							\
	$(NULL)

INCLUDES = \
	-I$(top_srcdir)							\
	-I$(top_builddir)						\
        -I$(top_srcdir)/components/services/trilobite			\
	$(GNOME_CFLAGS)							\
	$(GCONF_CFLAGS)							\
	$(GHTTP_CFLAGS)							\
	$(NULL)

noinst_LIBRARIES = libsummary.a

libsummary_a_SOURCES =							\
	eazel-summary-shared.h						\
	eazel-summary-shared.c						\
	$(NULL)

