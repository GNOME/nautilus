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
	-DLOG_DOMAIN=\"Nautilus-Installer\"			\
	-I.							\
	-I@top_srcdir@						\
	-I/usr/include/rpm					\
        -I@top_srcdir@/components/services/trilobite		\
	@GNOME_CFLAGS@						\
	@GCONF_CFLAGS@						\
	@XML_CFLAGS@						\
	@GHTTP_CFLAGS@						\
	$(NULL)

$(libeazelinstall_minimal_a_SOURCES):
	for i in $(top_srcdir)/components/services/install/lib/*.[ch]; \
	do bn=`basename $$i`; rm -f $$bn; ln -s $$i $$bn; done
