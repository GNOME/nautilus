NULL=

INCLUDES =\
	-I$(top_srcdir) 		\
	-I$(top_builddir) 		\
	-I$(top_builddir)/libnautilus 	\
	-I$(top_srcdir)/cut-n-paste-code\
	$(EEL_INCLUDEDIR) \
	$(LIBRSVG_INCLUDEDIR) \
	$(BONOBO_CFLAGS) 		\
	$(OAF_CFLAGS) 			\
	$(GCONF_CFLAGS) 		\
	$(GNOMEUI_CFLAGS) 		\
	$(VFS_CFLAGS) 			\
	$(XML_CFLAGS) 			\
	-DVERSION="\"$(VERSION)\"" 	\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\" 		\
	-DNAUTILUS_DATADIR=\""$(datadir)/nautilus"\" 		\
	$(NULL)
#	-DBACKGROUNDS_DIR=\""$(top_srcdir)/data/backgrounds"\"	\

LDADD =\
	$(top_builddir)/libnautilus/libnautilus.la \
	$(top_builddir)/libnautilus-extensions/libnautilus-extensions.la \
	$(EEL_LIBS) \
	$(LIBRSVG_LIBS) \
	$(BONOBO_LIBS) \
	$(OAF_LIBS) \
	$(GCONF_LIBS) \
	$(GNOMEUI_LIBS) \
	$(VFS_LIBS) \
	$(XML_LIBS) \
	$(NULL)

noinst_PROGRAMS =\
	test-nautilus-async-activation \
	test-nautilus-mime-actions \
	test-nautilus-mime-actions-set \
	test-nautilus-preferences-change \
	test-nautilus-preferences-display \
	test-nautilus-smooth-graphics \
	test-nautilus-wrap-table \
	$(NULL)

test_nautilus_async_activation_SOURCES = test-nautilus-async-activation.c test.c
test_nautilus_mime_actions_SOURCES = test-nautilus-mime-actions.c test.c
test_nautilus_mime_actions_set_SOURCES = test-nautilus-mime-actions-set.c test.c
test_nautilus_preferences_change_SOURCES = test-nautilus-preferences-change.c test.c
test_nautilus_preferences_display_SOURCES = test-nautilus-preferences-display.c test.c
test_nautilus_smooth_graphics_SOURCES = test-nautilus-smooth-graphics.c test.c
test_nautilus_wrap_table_SOURCES = test-nautilus-wrap-table.c test.c

EXTRA_DIST = \
	test.h \
	$(NULL)
