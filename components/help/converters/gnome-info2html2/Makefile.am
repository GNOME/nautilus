## Process this file with automake to produce Makefile.in.

INCLUDES = 			\
	-I$(includedir)		\
	-I$(top_srcdir)/intl


LDADD = \
	$(GNOME_LIBDIR)					\
	$(GNOMEUI_LIBS)					\
	$(INTLLIBS)

bin_PROGRAMS = gnome-info2html

gnome_info2html_SOURCES = main.c parse.c utils.c html.c

CFLAGS += -Wall


