NULL=

noinst_LTLIBRARIES = libgtkclist.la

INCLUDES = \
	-I$(top_srcdir)/cut-n-paste-code \
	$(GNOME_CFLAGS) \
	$(NULL)

libgtkclist_la_LDFLAGS = -static

libgtkclist_la_SOURCES = \
	gtkclist.c \
	$(NULL)

# Everything is private for now
noinst_HEADERS =\
	gtkclist.h \
	$(NULL)
