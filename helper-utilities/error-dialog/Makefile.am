NULL =

INCLUDES = \
	-DG_LOG_DOMAIN=\"NautilusErrorDialog\" \
	-I$(top_srcdir) \
	$(GNOMEUI_CFLAGS) \
        -DVERSION="\"$(VERSION)\"" \
	$(NULL)

bin_PROGRAMS = nautilus-error-dialog

nautilus_error_dialog_SOURCES =\
	nautilus-error-dialog.c \
	$(NULL)

nautilus_error_dialog_LDADD =	\
	$(GNOMEUI_LIBS)	\
	$(NULL)
