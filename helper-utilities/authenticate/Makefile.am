NULL =

CPPFLAGS = \
	-DPREFIX=\"$(prefix)\" \
	-DG_LOG_DOMAIN=\"Nautilus-Authenticate\"

INCLUDES = \
	-I$(top_srcdir) \
	$(GNOMEUI_CFLAGS) \
	$(NULL)

bin_PROGRAMS = nautilus-authenticate

nautilus_authenticate_SOURCES =\
	nautilus-authenticate.h \
	nautilus-authenticate.c \
	nautilus-authenticate-pam.c \
	nautilus-authenticate-fork.c \
	$(NULL)

nautilus_authenticate_LDADD =	\
	$(top_builddir)/libnautilus-extensions/libnautilus-extensions.la  \
	$(GNOMEUI_LIBS)	\
	$(GCONF_LIBS) \
	$(PAM_LIBS) \
	$(top_builddir)/libnautilus/libnautilus.la \
	$(BONOBO_LIBS) \
	$(NULL)
