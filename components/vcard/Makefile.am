NULL = 

SUBDIRS =

INCLUDES =					\
	-DPREFIX=\"$(prefix)\"			\
	-DG_LOG_DOMAIN=\"Nautilus-VCard\"	\
	-DDATADIR=\""$(datadir)"\" 		\
	-I$(top_srcdir)				\
	-I$(top_builddir)			\
	-I$(top_builddir)/libnautilus		\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\" \
	$(GNOMEUI_CFLAGS)                       \
	$(GCONF_CFLAGS) 			\
	$(OAF_CFLAGS)				\
	$(BONOBO_CFLAGS)			\
	$(VFS_CFLAGS)

oafdir = $(datadir)/oaf
oaf_DATA =					\
	nautilus-vcard.oaf


bin_PROGRAMS =					\
	nautilus-vcard

nautilus_vcard_SOURCES =		        \

	vcard.c					\
	vcard.h					\
	nautilus-vcard.c			\
	nautilus-vcard.h			\
	main.c

nautilus_vcard_LDADD =			\
	$(top_builddir)/libnautilus/libnautilus.la	\
	$(top_builddir)/libnautilus-extensions/libnautilus-extensions.la \
	$(BONOBO_LIBS)					\
	$(GNOMEUI_LIBS)					\
	$(GCONF_LIBS) 					\
	$(VFS_LIBS)					\
	$(GNORBA_LIBS)

OBJECT_DIRECTORY_LIBS = $(GNOME_LIBS) $(OAF_LIBS)

EXTRA_DIST = 			\
	$(oaf_DATA)		\
	$(NULL)
