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

$(libtrilobite_minimal_a_SOURCES):
	for i in $(top_srcdir)/components/services/trilobite/libtrilobite/*.[ch]; \
	do bn=`basename $$i`; rm -f $$bn; ln -s $$i $$bn; done
