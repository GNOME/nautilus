# FIXME I want the HTML component to be a shared library component, not an exe
# one.

CPPFLAGS =					\
	-DPREFIX=\"$(prefix)\"
	-DG_LOG_DOMAIN=\"Nautilus-HTML\"

INCLUDES =					\
	-I$(top_srcdir)/intl			\
	-I$(top_builddir)/intl			\
	-I$(top_srcdir)/src/lib			\
	-I$(top_builddir)/src/lib		\
	$(DMALLOC_INCLUDES)			\
	$(GNOME_INCLUDEDIR)

gnorbadir = $(sysconfdir)/CORBA/servers
gnorba_DATA =					\
	ntl-html-component.gnorba


bin_PROGRAMS =					\
	ntl-html-component

ntl_html_component_SOURCES =		\
	main.c

ntl_html_component_LDADD =			\
	$(top_builddir)/libnautilus/libnautilus.la	\
	-lgtkhtml -lgdk_pixbuf			\
	$(BONOBO_LIBS)				\
	$(GNOME_LIBDIR)				\
	$(GNOMEUI_LIBS)				\
	$(GNOME_VFS_LIBS)			\
	$(GNORBA_LIBS)				\
	$(GNOME_XML_LIB)			\
	$(INTLLIBS)


EXTRA_DIST = ntl-html-component.gnorba
