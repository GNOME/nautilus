NULL=

INCLUDES =\
	-I$(top_srcdir) 		\
	-I$(top_builddir) 		\
	-I$(top_builddir)/libnautilus 	\
	-I$(top_srcdir)/cut-n-paste-code\
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
	$(BONOBO_LIBS) \
	$(OAF_LIBS) \
	$(GCONF_LIBS) \
	$(GNOMEUI_LIBS) \
	$(VFS_LIBS) \
	$(XML_LIBS) \
	$(NULL)

noinst_PROGRAMS =\
	test-nautilus-async-activation \
	test-nautilus-background \
	test-nautilus-clickable-image \
	test-nautilus-font \
	test-nautilus-font-picker \
	test-nautilus-font-simple \
	test-nautilus-glyph \
	test-nautilus-glyph-simple \
	test-nautilus-image \
	test-nautilus-image-background \
	test-nautilus-image-scrolled \
	test-nautilus-image-simple \
	test-nautilus-image-table \
	test-nautilus-image-tile \
	test-nautilus-label \
	test-nautilus-label-background \
	test-nautilus-label-flavorful \
	test-nautilus-label-offset \
	test-nautilus-label-scrolled \
	test-nautilus-label-simple \
	test-nautilus-label-wrapped \
	test-nautilus-labeled-image \
	test-nautilus-mime-actions \
	test-nautilus-mime-actions-set \
	test-nautilus-password-dialog \
	test-nautilus-pixbuf-tile \
	test-nautilus-preferences-change \
	test-nautilus-preferences-display \
	test-nautilus-smooth-graphics \
	test-nautilus-widgets \
	test-nautilus-wrap-table \
	$(NULL)

test_nautilus_async_activation_SOURCES = test-nautilus-async-activation.c
test_nautilus_background_SOURCES = test-nautilus-background.c
test_nautilus_clickable_image_SOURCES = test-nautilus-clickable-image.c test.c
test_nautilus_font_SOURCES = test-nautilus-font.c test.c
test_nautilus_font_picker_SOURCES = test-nautilus-font-picker.c
test_nautilus_font_simple_SOURCES = test-nautilus-font-simple.c test.c
test_nautilus_glyph_SOURCES = test-nautilus-glyph.c test.c
test_nautilus_glyph_simple_SOURCES = test-nautilus-glyph-simple.c test.c
test_nautilus_image_SOURCES = test-nautilus-image.c
test_nautilus_image_background_SOURCES = test-nautilus-image-background.c test.c
test_nautilus_image_scrolled_SOURCES = test-nautilus-image-scrolled.c test.c
test_nautilus_image_simple_SOURCES = test-nautilus-image-simple.c
test_nautilus_image_table_SOURCES = test-nautilus-image-table.c test.c
test_nautilus_image_tile_SOURCES = test-nautilus-image-tile.c test.c
test_nautilus_label_SOURCES = test-nautilus-label.c
test_nautilus_label_background_SOURCES = test-nautilus-label-background.c test.c
test_nautilus_label_flavorful_SOURCES = test-nautilus-label-flavorful.c
test_nautilus_label_offset_SOURCES = test-nautilus-label-offset.c test.c
test_nautilus_label_scrolled_SOURCES = test-nautilus-label-scrolled.c test.c
test_nautilus_label_simple_SOURCES = test-nautilus-label-simple.c test.c
test_nautilus_label_wrapped_SOURCES = test-nautilus-label-wrapped.c
test_nautilus_labeled_image_SOURCES = test-nautilus-labeled-image.c test.c
test_nautilus_mime_actions_SOURCES = test-nautilus-mime-actions.c
test_nautilus_mime_actions_set_SOURCES = test-nautilus-mime-actions-set.c
test_nautilus_password_dialog_SOURCES = test-nautilus-password-dialog.c
test_nautilus_pixbuf_tile_SOURCES = test-nautilus-pixbuf-tile.c test.c
test_nautilus_preferences_change_SOURCES = test-nautilus-preferences-change.c test.c
test_nautilus_preferences_display_SOURCES = test-nautilus-preferences-display.c test.c
test_nautilus_smooth_graphics_SOURCES = test-nautilus-smooth-graphics.c
test_nautilus_widgets_SOURCES = test-nautilus-widgets.c
test_nautilus_wrap_table_SOURCES = test-nautilus-wrap-table.c test.c

EXTRA_DIST = \
	test.h \
	$(NULL)
