NULL=

bin_PROGRAMS = test-nautilus-widgets

test_nautilus_widgets_LDADD =\
	libnautilus-widgets.la \
	$(GNOMEUI_LIBS) \
	$(NULL)

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
	$(GNOME_LIBS)

libnautilus_widgetsincludedir=$(includedir)/nautilus-widgets

libnautilus_widgetsinclude_HEADERS =\
	nautilus-caption-table.h \
	nautilus-password-dialog.h \
	nautilus-preference.h \
	nautilus-preferences-box.h \
	nautilus-preferences-dialog.h \
	nautilus-preferences-group.h \
	nautilus-preferences-item.h \
	nautilus-preferences-pane.h \
	nautilus-preferences.h \
	nautilus-radio-button-group.h \
	$(NULL)

libnautilus_widgets_la_SOURCES =\
	nautilus-widgets-self-check-functions.h \
	nautilus-caption-table.c \
	nautilus-password-dialog.c \
	nautilus-preference.c \
	nautilus-preferences-box.c \
	nautilus-preferences-dialog.c \
	nautilus-preferences-group.c \
	nautilus-preferences-item.c \
	nautilus-preferences-pane.c \
	nautilus-preferences.c \
	nautilus-radio-button-group.c \
	nautilus-widgets-self-check-functions.c \
	$(NULL)

test_nautilus_widgets_SOURCES =\
	test-nautilus-widgets.c \
	$(NULL)

test_nautilus_widgets_LDADD =\
	$(GCONF_LIBS) \
	../nautilus-widgets/libnautilus-widgets.la \
	../libnautilus-extensions/libnautilus-extensions.la \
	../librsvg/librsvg.la \
	$(NULL)

#test_preferences_SOURCES =\
#	test-preferences.c \
#	$(NULL)

