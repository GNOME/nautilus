NULL=

bin_PROGRAMS=test-nautilus-widgets

test_nautilus_widgets_LDADD =\
	libnautilus-widgets.la \
	$(GNOMEUI_LIBS) \
	$(NULL)

lib_LTLIBRARIES=libnautilus-widgets.la

INCLUDES=-I$(top_srcdir) -I$(top_builddir) \
	$(GNOME_CFLAGS) \
	$(WERROR) \
	-D_REENTRANT

libnautilus_widgets_la_LDFLAGS=\
	$(GNOME_LIBS)

libnautilus_widgetsincludedir=$(includedir)/nautilus-widgets

libnautilus_widgetsinclude_HEADERS =\
	nautilus-radio-button-group.h \
	$(NULL)

libnautilus_widgets_la_SOURCES =\
	nautilus-radio-button-group.c \
	$(NULL)

test_nautilus_widgets_SOURCES =\
	test-nautilus-widgets.c \
	$(NULL)

