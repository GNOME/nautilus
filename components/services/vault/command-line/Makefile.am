NULL =

CPPFLAGS =								\
	-DPREFIX=\"$(prefix)\"
	-DG_LOG_DOMAIN=\"trilobite-eazel-vault-service\"

INCLUDES =								\
	-I$(top_srcdir)							\
	-I$(top_builddir)						\
	-I$(top_srcdir)/components/services/trilobite 			\
	-I$(top_srcdir)/components/services/vault/lib		 	\
	$(GNOMEUI_CFLAGS)                       			\
	$(GCONF_CFLAGS)							\
	$(NULL)

bin_PROGRAMS =								\
	eazel-vault

noinst_HEADERS = \
	vault-operations.h	\
	$(NULL)

eazel_vault_SOURCES =							\
	main.c								\
	vault-operations.c						\
	$(NULL)

eazel_vault_LDADD =							\
	$(BONOBO_LIBS)							\
	$(GNOMEUI_LIBS)							\
	$(GCONF_LIBS)							\
	$(VFS_LIBS)							\
	$(OAF_LIBS)							\
	$(top_builddir)/components/services/trilobite/libtrilobite/libtrilobite.la \
	$(NULL)

