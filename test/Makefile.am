NULL=

AM_CPPFLAGS =\
	-I$(top_srcdir) \
	$(BASE_CFLAGS) \
	$(COMMON_CFLAGS) \
	$(NAUTILUS_CFLAGS) \
	$(WARNING_CFLAGS) \
	-DVERSION="\"$(VERSION)\"" \
	-DNAUTILUS_DATADIR=\""$(datadir)/nautilus"\" \
	-DGNOMELOCALEDIR=\""$(prefix)/${DATADIRNAME}/locale"\"	\
	$(NULL)

LDADD =\
	$(top_builddir)/src/libnautilus.la \
	$(NULL)

noinst_PROGRAMS =\
	test-nautilus-search-engine \
	test-nautilus-directory-async \
	test-nautilus-copy \
	test-file-utilities-get-common-filename-prefix \
	test-eel-string-rtrim-punctuation \
	test-eel-string-get-common-prefix \
	$(NULL)

test_nautilus_copy_SOURCES = test-copy.c test.c

test_nautilus_search_engine_SOURCES = test-nautilus-search-engine.c 

test_nautilus_directory_async_SOURCES = test-nautilus-directory-async.c

test_file_utilities_get_common_filename_prefix_SOURCES = test-file-utilities-get-common-filename-prefix.c

test_eel_string_rtrim_punctuation_SOURCES = test-eel-string-rtrim-punctuation.c

test_eel_string_get_common_prefix_SOURCES = test-eel-string-get-common-prefix.c


TESTS = test-file-utilities-get-common-filename-prefix \
	test-eel-string-rtrim-punctuation \
	test-eel-string-get-common-prefix \
	$(NULL)

EXTRA_DIST = \
	test.h \
	$(NULL)

-include $(top_srcdir)/git.mk
