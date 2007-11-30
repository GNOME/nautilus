NULL=

INCLUDES =\
	-I$(top_srcdir) \
	$(CORE_CFLAGS) \
	-DVERSION="\"$(VERSION)\"" \
	-DNAUTILUS_DATADIR=\""$(datadir)/nautilus"\" \
	$(NULL)

LDADD =\
	$(top_builddir)/libnautilus-private/libnautilus-private.la \
	$(CORE_LIBS) \
	$(NULL)

noinst_PROGRAMS =\
	test-nautilus-wrap-table \
	test-nautilus-search-engine \
	test-nautilus-directory-async \
	test-nautilus-copy \
	$(NULL)

test_nautilus_copy_SOURCES = test-copy.c test.c

test_nautilus_wrap_table_SOURCES = test-nautilus-wrap-table.c test.c

test_nautilus_search_engine_SOURCES = test-nautilus-search-engine.c 

test_nautilus_directory_async_SOURCES = test-nautilus-directory-async.c

EXTRA_DIST = \
	test.h \
	$(NULL)
