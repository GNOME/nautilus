NULL=

bin_PROGRAMS = test-nautilus-widgets test-nautilus-preferences

lib_LTLIBRARIES=libnautilus-widgets.la

INCLUDES =\
	-I$(top_srcdir) \
	-I$(top_builddir) \
	$(GNOME_CFLAGS) \
	$(GCONF_CFLAGS) \
	$(WERROR) \
	-D_REENTRANT \
	$(NULL)

libnautilus_widgets_la_LDFLAGS=\
	$(GNOME_LIBS) \
	$(top_builddir)/libnautilus/.libs/libnautilus.so

libnautilus_widgetsincludedir=$(includedir)/nautilus-widgets

libnautilus_widgetsinclude_HEADERS =\
	nautilus-caption-table.h \
	nautilus-caption.h \
	nautilus-password-dialog.h \
	nautilus-preference.h \
	nautilus-preferences-box.h \
	nautilus-preferences-dialog.h \
	nautilus-preferences-group.h \
	nautilus-preferences-item.h \
	nautilus-preferences-pane.h \
	nautilus-preferences.h \
	nautilus-radio-button-group.h \
	nautilus-string-picker.h \
	nautilus-text-caption.h \
	nautilus-user-level-manager.h \
	$(NULL)

libnautilus_widgets_la_SOURCES =\
	nautilus-caption-table.c \
	nautilus-caption.c \
	nautilus-password-dialog.c \
	nautilus-preference.c \
	nautilus-preferences-box.c \
	nautilus-preferences-dialog.c \
	nautilus-preferences-group.c \
	nautilus-preferences-item.c \
	nautilus-preferences-pane.c \
	nautilus-preferences.c \
	nautilus-radio-button-group.c \
	nautilus-string-picker.c \
	nautilus-text-caption.c \
	nautilus-user-level-manager.c \
	nautilus-widgets-self-check-functions.c \
	nautilus-widgets-self-check-functions.h \
	$(NULL)

test_ldd =\
	$(GCONF_LIBS) \
	$(GNOMEUI_LIBS) \
	../nautilus-widgets/libnautilus-widgets.la \
	../libnautilus-extensions/libnautilus-extensions.la \
	../libnautilus/libnautilus.la	\
	../librsvg/librsvg.la \
	$(NULL)

test_nautilus_widgets_SOURCES =\
	test-nautilus-widgets.c \
	$(NULL)

test_nautilus_widgets_LDADD = $(test_ldd)

test_nautilus_preferences_SOURCES =\
	test-nautilus-preferences.c \
	$(NULL)

test_nautilus_preferences_LDADD = $(test_ldd)

../nautilus-widgets/libnautilus-widgets.la: libnautilus-widgets.la
