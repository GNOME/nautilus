NULL =

CPPFLAGS = 								\
	-DPREFIX=\"$(prefix)\"						\
	-DG_LOG_DOMAIN=\"trilobite-eazel-vault\"			\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\" 			\
	$(NULL)

INCLUDES =								\
	-I$(top_srcdir) 						\
	-I$(top_builddir) 						\
	-I$(top_srcdir)/components/services/trilobite			\
	-I$(top_srcdir)/components/services/vault/lib			\
	$(GCONF_CFLAGS) 						\
	$(GNOME_CFLAGS) 						\
	$(GNOME_INCLUDEDIR) 						\
	$(OAF_CFLAGS) 							\
	$(XML_CFLAGS) 							\
	$(WERROR) 							\
	$(NULL)

LDFLAGS = 								\
	$(GLIB_LIBS)							\
	$(OAF_LIBS) 							\
	$(NULL)


oafdir = $(datadir)/oaf

oaf_DATA = trilobite-eazel-vault-service.oafinfo

bin_PROGRAMS = trilobite-eazel-vault-service

trilobite_eazel_vault_service_SOURCES =					\
	trilobite-eazel-vault-impl.c					\
	main.c								\
	$(NULL)

trilobite_eazel_vault_service_LDADD =					\
	$(top_builddir)/components/services/trilobite/libtrilobite/libtrilobite.la \
	$(top_builddir)/components/services/vault/lib/libvault_corba.a	\
	$(NULL)

EXTRA_DIST = 								\
	$(oaf_DATA)							\
	$(NULL)
