NULL=

INCLUDES =\
	-I$(top_srcdir) \
	-I$(top_builddir) \
	-I$(top_builddir)/libnautilus \
	$(BONOBO_CFLAGS) \
	$(OAF_CFLAGS) \
	$(GCONF_CFLAGS) \
	$(GNOMEUI_CFLAGS) \
	$(GNOMECANVASPIXBUF_INCLUDEDIR) \
	$(VFS_CFLAGS) \
	$(XML_CFLAGS) \
	-DVERSION="\"$(VERSION)\"" \
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\" \
	$(WERROR) \
	$(NULL)

LDADD =\
	../nautilus-widgets/libnautilus-widgets.la \
	../libnautilus/libnautilus.la \
	../libnautilus-extensions/libnautilus-extensions.la \
	../librsvg/librsvg.la \
	$(BONOBO_LIBS) \
	$(OAF_LIBS) \
	$(GCONF_LIBS) \
	$(GNOMEUI_LIBS) \
	$(GNOMECANVASPIXBUF_LIBS) \
	$(VFS_LIBS) \
	$(XML_LIBS) \
	$(NULL)

noinst_PROGRAMS =				\
	test-nautilus-mime-actions		\
	test-nautilus-mime-actions-set		\
	$(NULL)

test_nautilus_mime_actions_SOURCES = test-nautilus-mime-actions.c

test_nautilus_mime_actions_set_SOURCES = test-nautilus-mime-actions-set.c

EXTRA_DIST =					\
	test-nautilus-mime-actions.c		\
	$(NULL)
