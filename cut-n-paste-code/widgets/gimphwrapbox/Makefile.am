NULL=

noinst_LTLIBRARIES = libgtkhwrapbox.la

INCLUDES = \
	-I$(top_srcdir)/cut-n-paste-code \
	$(GNOMEUI_CFLAGS) \
	$(NULL)

libgtkhwrapbox_la_LDFLAGS = -static

libgtkhwrapbox_la_SOURCES = \
	gtkhwrapbox.c \
	gtkwrapbox.c \
	$(NULL)

# Everything is private for now
noinst_HEADERS =\
	gtkhwrapbox.h \
	gtkwrapbox.h \
	$(NULL)
