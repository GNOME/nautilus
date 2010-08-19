NULL=

INCLUDES =\
	-I$(top_srcdir) \
	$(CORE_CFLAGS) \
	$(WARNING_CFLAGS) \
	-DVERSION="\"$(VERSION)\"" \
	-DNAUTILUS_DATADIR=\""$(datadir)/nautilus"\" \
	-DGNOMELOCALEDIR=\""$(prefix)/${DATADIRNAME}/locale"\"	\
	$(NULL)

LDADD =\
	$(top_builddir)/libnautilus-private/libnautilus-private.la \
	$(CORE_LIBS) \
	$(NULL)

noinst_PROGRAMS =\
	test-nautilus-search-engine \
	test-nautilus-directory-async \
	test-nautilus-copy \
	test-eel-background \
	test-eel-editable-label	\
	$(NULL)

test_nautilus_copy_SOURCES = test-copy.c test.c

test_nautilus_search_engine_SOURCES = test-nautilus-search-engine.c 

test_nautilus_directory_async_SOURCES = test-nautilus-directory-async.c

test_eel_background_SOURCES = test-eel-background.c

EXTRA_DIST = \
	test.h \
	$(NULL)
