NULL=

INCLUDES =\
	-I$(top_srcdir) \
	-I$(top_builddir) \
	-I$(top_builddir)/libnautilus \
	$(BONOBO_CFLAGS) \
	$(OAF_CFLAGS) \
	$(GCONF_CFLAGS) \
	$(GNOMEUI_CFLAGS) \
	$(VFS_CFLAGS) \
	$(XML_CFLAGS) \
	-DVERSION="\"$(VERSION)\"" \
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\" \
	$(NULL)

# for now just hardcode a plain old link line, someday someone can 
#figure out what the magic password is to make libtool do this cleanly

all: libleakcheck.so
CLEANFILES += libleakcheck.so

libleakcheck.so: nautilus-leak-checker.c nautilus-leak-hash-table.c nautilus-leak-symbol-lookup.c
	gcc $(srcdir)/nautilus-leak-checker.c $(srcdir)/nautilus-leak-hash-table.c \
		$(srcdir)/nautilus-leak-symbol-lookup.c \
		-Wall -o $@ -shared $(GLIB_CFLAGS) $(GLIB_LIBS) -lpthread -ldl -Wl,-Bstatic -lbfd -liberty -Wl,-Bdynamic

# to include them in "make dist"
EXTRA_DIST = nautilus-leak-checker.c nautilus-leak-hash-table.c nautilus-leak-symbol-lookup.c \
	nautilus-leak-checker-stubs.h nautilus-leak-hash-table.h nautilus-leak-checker.h nautilus-leak-symbol-lookup.h

leakcheck_DATA = libleakcheck.so
leakcheckdir= $(DESTDIR)$(libdir)

leakchecktest: nautilus-leak-checker.c nautilus-leak-hash-table.c nautilus-leak-symbol-lookup.c
	gcc $(srcdir)/nautilus-leak-checker.c $(srcdir)/nautilus-leak-hash-table.c \
		$(srcdir)/nautilus-leak-symbol-lookup.c \
		-Wall -o $@ -DLEAK_CHECK_TESTING $(GLIB_CFLAGS) $(GLIB_LIBS) -lpthread -ldl -Wl,-Bstatic -lbfd -liberty -Wl,-Bdynamic
