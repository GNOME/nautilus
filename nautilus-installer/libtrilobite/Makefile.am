NULL =

if ENABLE_INSTALLER
LIBRARY = libtrilobite_minimal.a
endif

noinst_LIBRARIES = $(LIBRARY)

libtrilobite_minimal_a_SOURCES =	\
	trilobite-core-distribution.c	\
	trilobite-root-helper.c		\
	trilobite-core-utils.c		\
	trilobite-core-network-slim.c	\
	trilobite-core-messaging.c	\
	trilobite-md5-tools.c		\
	$(NULL)

HDRS = \
	libtrilobite.h \
	libtrilobite-service.h \
	trilobite-core-distribution.h \
	trilobite-core-messaging.h \
	trilobite-core-network.h \
	trilobite-core-utils.h \
	trilobite-md5-tools.h \
	trilobite-redirect.h \
	trilobite-root-client-private.h \
	trilobite-root-client-public.h \
	trilobite-root-helper.h \
	trilobite-service.h \
	trilobite-service-passwordquery-private.h \
	trilobite-service-passwordquery-public.h \
	trilobite-service-private.h \
	trilobite-service-public.h \
	$(NULL)

INCLUDES=							\
	-DTRILOBITE_VERSION=\"0.1.0\"				\
	-I.							\
	-I@top_srcdir@						\
        -I@top_srcdir@/components/services/trilobite		\
	-DTRILOBITE_SLIM 					\
	-DLOG_DOMAIN=\"Nautilus-Installer\"			\
	@GNOME_CFLAGS@						\
	@XML_CFLAGS@						\
	$(NULL)

$(libtrilobite_minimal_a_SOURCES) $(HDRS):
	for i in $(top_srcdir)/components/services/trilobite/libtrilobite/*.[ch]; \
	do bn=`basename $$i`; rm -f $$bn; ln -s $$i $$bn; done

