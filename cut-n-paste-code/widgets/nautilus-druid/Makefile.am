NULL=

noinst_LTLIBRARIES = libnautilus-druid.la

INCLUDES = \
	-I$(top_srcdir)/cut-n-paste-code \
	$(GNOME_CFLAGS) \
	$(GDK_PIXBUF_CFLAGS) \
	$(NULL)

libnautilus_druid_la_LDFLAGS = -static

libnautilus_druid_la_SOURCES = \
	nautilus-druid-page-edge.c \
	nautilus-druid-page-finish.c \
	nautilus-druid-page-standard.c \
	nautilus-druid-page-start.c \
	nautilus-druid-page.c \
	nautilus-druid.c \
	$(NULL)

# Everything is private for now
noinst_HEADERS =\
	nautilus-druid-page-edge.h \
	nautilus-druid-page-finish.h \
	nautilus-druid-page-standard.h \
	nautilus-druid-page-start.h \
	nautilus-druid-page.h \
	nautilus-druid.h \
	$(NULL)
