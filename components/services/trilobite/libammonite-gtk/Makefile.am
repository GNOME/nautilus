SUBDIRS = 
NULL =

lib_LIBRARIES = libammonite-gtk.a

libammonite_gtkincludedir = $(includedir)/libtrilobite

libammonite_gtkinclude_HEADERS = \
	ammonite-login-dialog.h	\
	libammonite-gtk.h		\
	$(NULL)

libammonite_gtk_a_SOURCES = \
	ammonite-login-dialog.c 	\
	libammonite-gtk.c		\
	$(NULL)

INCLUDES = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/libnautilus-extensions \
	-I$(top_builddir) \
	$(GLIB_CFLAGS)	\
	$(OAF_CFLAGS)	\
	$(GNOMEUI_CFLAGS) \
	$(WERROR)	\
	-DDATADIR=\""$(datadir)"\" \
	$(AMMONITE_CFLAGS) \
	$(NULL)
