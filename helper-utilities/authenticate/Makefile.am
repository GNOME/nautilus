NULL =

CPPFLAGS = \
	-DPREFIX=\"$(prefix)\" \
	-DG_LOG_DOMAIN=\"Nautilus-Authenticate\"

INCLUDES = \
	-I$(top_srcdir) \
	$(GNOMEUI_CFLAGS) \
	$(WERROR) \
	$(NULL)

bin_PROGRAMS = nautilus-authenticate

nautilus_authenticate_SOURCES =\
	nautilus-authenticate.h \
	nautilus-authenticate.c \
	nautilus-authenticate-pam.c \
	nautilus-authenticate-fork.c \
	$(NULL)

nautilus_authenticate_LDFLAGS =	\
	$(top_builddir)/nautilus-widgets/libnautilus-widgets.la \
	$(top_builddir)/libnautilus-extensions/libnautilus-extensions.la \
	$(top_builddir)/librsvg/librsvg.la \
	$(GNOMEUI_LIBS)	\
	$(GCONF_LIBS) \
	-lpam \
	-lpam_misc \
	$(NULL)
