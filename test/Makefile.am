NULL=

#lib_LTLIBRARIES = libleakcheck.la

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
	$(WERROR) \
	$(NULL)

LDADD =\
	$(top_builddir)/libnautilus/libnautilus.la \
	$(top_builddir)/libnautilus-extensions/libnautilus-extensions.la  \
	$(top_builddir)/cut-n-paste-code/widgets/e-paned/libe-paned.la \
	$(top_builddir)/librsvg/librsvg.la                                \
	$(BONOBO_LIBS) \
	$(OAF_LIBS) \
	$(GCONF_LIBS) \
	$(GNOMEUI_LIBS) \
	$(VFS_LIBS) \
	$(XML_LIBS) \
	$(NULL)

noinst_PROGRAMS =				\
	test-nautilus-mime-actions		\
	test-nautilus-mime-actions-set		\
	$(NULL)

test_nautilus_mime_actions_SOURCES = test-nautilus-mime-actions.c

test_nautilus_mime_actions_set_SOURCES = test-nautilus-mime-actions-set.c

#libleakcheck_la_SOURCES =    		\
#	nautilus-leak-checker.c		\
#	nautilus-leak-checker.h		\
#	nautilus-leak-checker-stubs.h	\
#	nautilus-leak-hash-table.c	\
#	nautilus-leak-hash-table.h	\
#	nautilus-leak-symbol-lookup.c	\
#	nautilus-leak-symbol-lookup.h	\
#	$(NULL)

EXTRA_DIST =					\
	test-nautilus-mime-actions.c		\
	$(NULL)

# the libtool way of building libleakcheck.so is totally broken, libtool cannot
# build an .so with statically linked -liberty

# LIBLEAKCHECK_STATIC_LIBS = "-Wl,-Bstatic -lbfd -liberty -Wl,-Bdynamic"
#libleakcheck_la_LDFLAGS = -module -avoid-version $(LIBLEAKCHECK_STATIC_LIBS)
#libleakcheck_la_LIBADD = -ldl 



# libleakcheck.la: $(libleakcheck_la_OBJECTS) $(libleakcheck_la_DEPENDENCIES)
#	$(LINK) -rpath $(libdir) $(libleakcheck_la_LDFLAGS) $(libleakcheck_la_OBJECTS) $(libleakcheck_la_LIBADD) $(LIBS)


# for now just hardcode a plain old link line, someday someone can 
#figure out what the magic password is to make libtool do this cleanly

all: libleakcheck.so
CLEANFILES += libleakcheck.so

libleakcheck.so: nautilus-leak-checker.c nautilus-leak-hash-table.c nautilus-leak-symbol-lookup.c
	gcc nautilus-leak-checker.c nautilus-leak-hash-table.c nautilus-leak-symbol-lookup.c -Wall -o $@ -shared $(GLIB_CFLAGS) $(GLIB_LIBS) -lpthread -ldl -Wl,-Bstatic -lbfd -liberty -Wl,-Bdynamic

leakcheck_DATA = libleakcheck.so
leakcheckdir= $(DESTDIR)$(libdir)


