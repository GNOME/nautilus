NULL =

CPPFLAGS =					\
	-DPREFIX=\"$(prefix)\"
	-DG_LOG_DOMAIN=\"Nautilus-Services\"

INCLUDES =					\
	-I$(top_srcdir)				\
	-I$(top_builddir)			\
	$(GNOMEUI_CFLAGS)                       \
	$(GCONF_CFLAGS)				\
	$(WERROR)				\
	$(NULL)

oafdir = $(datadir)/oaf

bin_PROGRAMS =					\
	nautilus-eazel-sample-service-cli

nautilus_eazel_sample_service_cli_SOURCES =	\
	sample-service-cli.c			\
	$(NULL)

nautilus_eazel_sample_service_cli_LDFLAGS =	\
	$(BONOBO_LIBS)				\
	$(GNOMEUI_LIBS)				\
	$(GCONF_LIBS)				\
	$(VFS_LIBS)				\
	$(OAF_LIBS)				\
	$(NULL)

