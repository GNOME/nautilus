NULL =

INCLUDES = \
	-DPREFIX=\"$(prefix)\" 						\
	-DG_LOG_DOMAIN=\"trilobite-eazel-summary\"			\
	-DSTANDALONE							\
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

