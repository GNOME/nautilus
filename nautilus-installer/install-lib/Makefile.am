NULL =

if ENABLE_INSTALLER
LIBRARY = libeazelinstall_minimal.a
endif

noinst_LIBRARIES = $(LIBRARY)

libeazelinstall_minimal_a_SOURCES = 		\
	eazel-install-metadata.c		\
	eazel-install-protocols.c		\
	eazel-install-logic2.c			\
	eazel-install-xml-package-list.c	\
	eazel-install-object.c			\
	eazel-install-problem.c			\
	eazel-softcat.c				\
	eazel-package-system.c			\
	eazel-package-system-types.c		\
	$(NULL)

libeazelinstall_minimal_a_LIBADD = $(PACKAGE_SYSTEM_OBJECT)
libeazelinstall_minimal_a_DEPENDENCIES = $(PACKAGE_SYSTEM_OBJECT)

INCLUDES=\
	-DEAZEL_INSTALL_NO_CORBA				\
	-DEAZEL_INSTALL_SLIM 					\
	-DG_LOG_DOMAIN=\"Nautilus-Installer\"			\
	-I.							\
	-I@top_srcdir@						\
	-I/usr/include/rpm					\
        -I@top_srcdir@/components/services/trilobite		\
        -I@top_srcdir@/components/services/trilobite/libtrilobite	\
        -I@top_srcdir@/components/services/install/lib		\
	@GNOME_CFLAGS@						\
	@GCONF_CFLAGS@						\
	@XML_CFLAGS@						\
	@GHTTP_CFLAGS@						\
	$(NULL)

TRILOBITE_HDRS =  \
	libtrilobite.h \
	libtrilobite-service.h \
	trilobite-core-distribution.h \
	trilobite-core-messaging.h \
	trilobite-core-network.h \
	trilobite-core-utils.h \
	trilobite-file-utilities.h \
	trilobite-md5-tools.h \
	trilobite-redirect.h \
	trilobite-root-client-private.h \
	trilobite-root-client-public.h \
	trilobite-root-helper.h \
	trilobite-service-passwordquery-private.h \
	trilobite-service-passwordquery-public.h \
	trilobite-service-private.h \
	trilobite-service-public.h \
	$(NULL)

$(TRILOBITE_HDRS):
	rm -f $@; ln $(top_srcdir)/components/services/trilobite/libtrilobite/`basename $@` $@

$(libeazelinstall_minimal_a_SOURCES) eazel-package-system-rpm4.c eazel-package-system-rpm3.c: $(TRILOBITE_HDRS)
	rm -f $@; ln $(top_srcdir)/components/services/install/lib/`basename $@` $@

eazel-package-system-rpm4.o: eazel-package-system-rpm4.c
eazel-package-system-rpm3.o: eazel-package-system-rpm3.c
