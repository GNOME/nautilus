NULL=

bin_PROGRAMS=test-nautilus-widgets # test-preferences

test_nautilus_widgets_LDADD =\
	libnautilus-widgets.la \
	$(GNOMEUI_LIBS) \
	$(NULL)

#test_preferences_LDADD =\
#	libnautilus-widgets.la \
#	$(GNOMEUI_LIBS) \
#	$(NULL)

lib_LTLIBRARIES=libnautilus-widgets.la

INCLUDES=-I$(top_srcdir) -I$(top_builddir) \
	$(GNOME_CFLAGS) \
	$(WERROR) \
	-D_REENTRANT

libnautilus_widgets_la_LDFLAGS=\
	$(GNOME_LIBS)

libnautilus_widgetsincludedir=$(includedir)/nautilus-widgets

libnautilus_widgetsinclude_HEADERS =\
	nautilus-preferences-box.h \
	nautilus-preferences-dialog.h \
	nautilus-preferences-group.h \
	nautilus-preferences-item.h \
	nautilus-preferences-pane.h \
	nautilus-preferences.h \
	nautilus-radio-button-group.h \
	$(NULL)

libnautilus_widgets_la_SOURCES =\
	nautilus-preferences-box.c \
	nautilus-preferences-dialog.c \
	nautilus-preferences-group.c \
	nautilus-preferences-item.c \
	nautilus-preferences-pane.c \
	nautilus-preferences.c \
	nautilus-radio-button-group.c \
	$(NULL)

test_nautilus_widgets_SOURCES =\
	test-nautilus-widgets.c \
	$(NULL)

#test_preferences_SOURCES =\
#	test-preferences.c \
#	$(NULL)

