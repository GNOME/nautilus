NULL=

INCLUDES =\
	-I$(top_srcdir) \
	$(CORE_CFLAGS) \
	-DVERSION="\"$(VERSION)\"" \
	-DGNOMELOCALEDIR=\""$(prefix)/${DATADIRNAME}/locale"\"	\
	-DNAUTILUS_DATADIR=\""$(datadir)/nautilus"\" \
	$(NULL)

LDADD =\
	$(top_builddir)/libnautilus/libnautilus.la \
	$(top_builddir)/libnautilus-private/libnautilus-private.la \
	$(CORE_LIBS) \
	$(NULL)

noinst_PROGRAMS =\
	test-nautilus-mime-actions \
	test-nautilus-mime-actions-set \
	test-nautilus-wrap-table \
	$(NULL)

test_nautilus_mime_actions_SOURCES = test-nautilus-mime-actions.c test.c
test_nautilus_mime_actions_set_SOURCES = test-nautilus-mime-actions-set.c test.c
test_nautilus_wrap_table_SOURCES = test-nautilus-wrap-table.c test.c

EXTRA_DIST = \
	test.h \
	$(NULL)
