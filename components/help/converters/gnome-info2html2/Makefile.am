## Process this file with automake to produce Makefile.in.

INCLUDES = 			\
	-I$(includedir)		\
	-I$(top_srcdir)/intl -I$(top_builddir)/intl	\
	$(GNOMEUI_CFLAGS)	

LDADD = \
	$(GNOME_LIBS) $(Z_LIBS) $(BZ_LIBS)

bin_PROGRAMS = gnome-info2html2

gnome_info2html2_SOURCES = main.c parse.c utils.c html.c data.h html.h parse.h utils.h version.h
