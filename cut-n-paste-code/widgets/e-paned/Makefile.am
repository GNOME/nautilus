NULL=

noinst_LTLIBRARIES = libe-paned.la

INCLUDES = \
	-I$(top_srcdir)/cut-n-paste-code \
	$(GNOME_CFLAGS) \
	$(NULL)

libe_paned_la_LDFLAGS = -static

libe_paned_la_SOURCES = \
	e-hpaned.c \
	e-paned.c \
	e-vpaned.c \
	$(NULL)

# Everything is private for now
noinst_HEADERS =\
	e-hpaned.h \
	e-paned.h \
	e-vpaned.h \
	$(NULL)
