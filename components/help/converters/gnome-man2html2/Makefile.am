## Process this file with automake to produce Makefile.in.

INCLUDES = 			\
	-I$(includedir)		\
	-I$(top_srcdir)/intl -I$(top_builddir)/intl


LDADD = \
	$(GNOME_LIBDIR)					\
	$(GNOMEUI_LIBS)					\
	$(INTLLIBS)

bin_PROGRAMS = gnome-man2html

gnome_man2html_SOURCES = gnome-man2html.c

CFLAGS += -Wall

