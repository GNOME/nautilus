NULL =

INCLUDES = \
	-DPREFIX=\"$(prefix)\" \
	-DG_LOG_DOMAIN=\"NautilusErrorDialog\" \
	-I$(top_srcdir) \
	$(GNOMEUI_CFLAGS) \
	$(NULL)

bin_PROGRAMS = nautilus-error-dialog

nautilus_error_dialog_SOURCES =\
	nautilus-error-dialog.c \
	$(NULL)

nautilus_error_dialog_LDADD =	\
	$(top_builddir)/libnautilus-extensions/libnautilus-extensions.la  \
	$(GNOMEUI_LIBS)	\
	$(GCONF_LIBS) \
	$(PAM_LIBS) \
	$(top_builddir)/libnautilus/libnautilus.la \
	$(BONOBO_LIBS) \
	$(NULL)
