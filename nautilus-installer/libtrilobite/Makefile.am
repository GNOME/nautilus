NULL =

if ENABLE_INSTALLER
LIBRARY = libtrilobite_minimal.a
endif

CFLAGS = -Wall -Werror -g -O2

noinst_LIBRARIES = $(LIBRARY)

libtrilobite_minimal_a_SOURCES =	\
	trilobite-core-distribution.c	\
	trilobite-root-helper.c		\
	trilobite-core-utils.c		\
	trilobite-core-network-slim.c	\
	trilobite-core-messaging.c	\
	trilobite-md5-tools.c		\
	$(NULL)

INCLUDES=								\
	-DTRILOBITE_VERSION=\"0.1.0\"					\
	-I.								\
	-I@top_srcdir@							\
        -I@top_srcdir@/components/services/trilobite			\
        -I@top_srcdir@/components/services/trilobite/libtrilobite	\
	-DTRILOBITE_SLIM						\
	-DG_LOG_DOMAIN=\"Nautilus-Installer\"				\
	@GLIB_CFLAGS@							\
	@GTK_CFLAGS@							\
	@XML_CFLAGS@							\
	$(NULL)

TRILOBITE_HDRS =  \
	libtrilobite.h \
	trilobite-core-distribution.h \
	trilobite-core-messaging.h \
	trilobite-core-network.h \
	trilobite-core-utils.h \
	trilobite-file-utilities.h \
	trilobite-i18n-utils.h \
	trilobite-md5-tools.h \
	trilobite-redirect.h \
	trilobite-root-client-private.h \
	trilobite-root-client-public.h \
	trilobite-root-helper.h \
	$(NULL)

$(TRILOBITE_HDRS):
	rm -f $@; ln -s $(top_srcdir)/components/services/trilobite/libtrilobite/`basename $@` $@

$(libtrilobite_minimal_a_SOURCES): $(TRILOBITE_HDRS)
	rm -f $@; ln -s $(top_srcdir)/components/services/trilobite/libtrilobite/`basename $@` $@
