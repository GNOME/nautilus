NULL=

lib_LTLIBRARIES = libleakcheck.la

INCLUDES =\
	-I$(top_srcdir) \
	-I$(top_builddir) \
	-I$(top_builddir)/libnautilus \
	$(BONOBO_CFLAGS) \
	$(OAF_CFLAGS) \
	$(GCONF_CFLAGS) \
	$(GNOMEUI_CFLAGS) \
	$(GNOMECANVASPIXBUF_INCLUDEDIR) \
	$(VFS_CFLAGS) \
	$(XML_CFLAGS) \
	-DVERSION="\"$(VERSION)\"" \
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\" \
	$(WERROR) \
	$(NULL)

LDADD =\
	../libnautilus/libnautilus.la \
	../libnautilus-extensions/libnautilus-extensions.la \
	../librsvg/librsvg.la \
	$(BONOBO_LIBS) \
	$(OAF_LIBS) \
	$(GCONF_LIBS) \
	$(GNOMEUI_LIBS) \
	$(GNOMECANVASPIXBUF_LIBS) \
	$(VFS_LIBS) \
	$(XML_LIBS) \
	$(NULL)

noinst_PROGRAMS =				\
	test-nautilus-mime-actions		\
	test-nautilus-mime-actions-set		\
	$(NULL)

test_nautilus_mime_actions_SOURCES = test-nautilus-mime-actions.c

test_nautilus_mime_actions_set_SOURCES = test-nautilus-mime-actions-set.c

libleakcheck_la_SOURCES =    		\
	nautilus-leak-checker.c		\
	nautilus-leak-checker.h		\
	nautilus-leak-checker-stubs.h	\
	nautilus-leak-hash-table.c	\
	nautilus-leak-hash-table.h	\
	nautilus-leak-symbol-lookup.c	\
	nautilus-leak-symbol-lookup.h	\
	$(NULL)

# we need to link libbfd and liberty statically
# this is currently broken because of libtool dumbness

# LIBLEAKCHECK_STATIC_LIBS = "-Wl,-Bstatic -lbfd -liberty -Wl,-Bdynamic"

libleakcheck_la_LDFLAGS = -module -avoid-version $(LIBLEAKCHECK_STATIC_LIBS)
libleakcheck_la_LIBADD = -ldl 

EXTRA_DIST =					\
	test-nautilus-mime-actions.c		\
	$(NULL)

# the link rule is here so we can remove  -rpath $(libdir)
# however - 
# if we leave -rpath $(libdir) here, libtool refuses to link with -liberty
# if we dont, .libs/libleakcheck.lai doesn't get generated and make install fails

libleakcheck.la: $(libleakcheck_la_OBJECTS) $(libleakcheck_la_DEPENDENCIES)
	$(LINK) -rpath $(libdir) $(libleakcheck_la_LDFLAGS) $(libleakcheck_la_OBJECTS) $(libleakcheck_la_LIBADD) $(LIBS)
