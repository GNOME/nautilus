NULL =

SUBDIRS=icons

CPPFLAGS =							\
	-DPREFIX=\"$(prefix)\" 					\
	-DG_LOG_DOMAIN=\"eazel-service\"			\
	$(NULL)

CFLAGS=-g -O

INCLUDES = \
	-I$(top_srcdir)						\
	-I$(top_builddir)					\
        -I$(top_srcdir)/components/services/trilobite		\
        -I$(top_builddir)/components/services/trilobite		\
        -I$(top_srcdir)/components/services/trilobite/lib	\
        -I$(top_builddir)/components/services/trilobite/lib	\
	$(GNOME_CFLAGS)						\
	$(GCONF_CFLAGS)						\
	$(XML_CFLAGS)						\
	$(NULL)

noinst_LIBRARIES = libnautilusdepshared.a

noinst_HEADERS = 						\
	shared-service-utilities.h				\
	shared-service-widgets.h				\
	$(NULL)

libnautilusdepshared_a_SOURCES = 				\
	shared-service-utilities.c				\
	shared-service-widgets.c				\
	$(NULL)

