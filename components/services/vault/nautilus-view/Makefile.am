NULL =

SUBDIR = icons

CPPFLAGS =							\
	-DPREFIX=\"$(prefix)\"					\
	 -DG_LOG_DOMAIN=\"trilobite-eazel-vault\"		\
	$(NULL)

INCLUDES =								\
	-I$(top_srcdir)							\
	-I$(top_srcdir)/components/services/trilobite/libtrilobite	\
	-I$(top_srcdir)/components/services/vault/lib			\
	-I$(top_builddir)						\
	$(GNOMEUI_CFLAGS)                       			\
	$(GCONF_CFLAGS) 						\
	$(NULL)

oafdir = $(datadir)/oaf

oaf_DATA =								\
	nautilus-vault-view.oafinfo


bin_PROGRAMS =								\
	nautilus-vault-view

nautilus_vault_view_SOURCES =				\
	nautilus-vault-view.h					\
	nautilus-vault-view.c		    			\
	main.c								\
	$(NULL)

nautilus_vault_view_LDADD =				\
	$(top_builddir)/components/services/vault/lib/libvault_corba.a  \
	$(top_builddir)/components/services/trilobite/libtrilobite/libtrilobite.la \
	$(top_builddir)/libnautilus/libnautilus.la			\
	$(top_builddir)/libnautilus-extensions/libnautilus-extensions.la	\
	$(BONOBO_LIBS)							\
	$(GNOMEUI_LIBS)							\
	$(GCONF_LIBS)							\
	$(VFS_LIBS)							\
	$(GHTTP_LIBS)							\
	$(OAF_LIBS)							\
	$(NULL)

EXTRA_DIST = $(oaf_DATA)
