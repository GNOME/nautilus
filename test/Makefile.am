NULL=

INCLUDES =\
	-I$(top_srcdir) \
	-I$(top_builddir) \
	-I$(top_builddir)/libnautilus \
	$(BONOBO_CFLAGS) \
	$(OAF_CFLAGS) \
	$(GCONF_CFLAGS) \
	$(GNOMEUI_CFLAGS) \
	$(VFS_CFLAGS) \
	$(XML_CFLAGS) \
	-DVERSION="\"$(VERSION)\"" \
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\" \
	$(NULL)

LDADD =\
	$(top_builddir)/libnautilus/libnautilus.la \
	$(top_builddir)/libnautilus-extensions/libnautilus-extensions.la  \
	$(BONOBO_LIBS) \
	$(OAF_LIBS) \
	$(GCONF_LIBS) \
	$(GNOMEUI_LIBS) \
	$(VFS_LIBS) \
	$(XML_LIBS) \
	$(NULL)

noinst_PROGRAMS =\
	test-nautilus-image \
	test-nautilus-mime-actions \
	test-nautilus-mime-actions-set \
	test-nautilus-password-dialog \
	test-nautilus-preferences \
	test-nautilus-widgets \
	$(NULL)

test_nautilus_mime_actions_SOURCES = test-nautilus-mime-actions.c

test_nautilus_mime_actions_set_SOURCES = test-nautilus-mime-actions-set.c

test_nautilus_widgets_SOURCES = test-nautilus-widgets.c

test_nautilus_preferences_SOURCES = test-nautilus-preferences.c

test_nautilus_image_SOURCES = test-nautilus-image.c

test_nautilus_password_dialog_SOURCES = test-nautilus-password-dialog.c

EXTRA_DIST =					\
	test-nautilus-mime-actions.c		\
	$(NULL)
