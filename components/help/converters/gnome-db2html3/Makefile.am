SUBDIRS= docbook gnome-customization

INCLUDES = \
        -DGNOMELOCALEDIR=\""$(datadir)/locale"\"        \
	$(GNOMEUI_CFLAGS)                               \
	-DPREFIXDIR=\""$(prefix)"\" 			\
	$(GNOME_DB2HTML3_FLAG)				\
	$(XML2_CFLAGS)					\
	$(XSLT_CFLAGS)

LDADD = \
	$(GNOME_LIBS) $(XML2_LIBS) $(XSLT_LIBS)

bin_PROGRAMS = gnome-db2html3

gnome_db2html3_SOURCES = \
	gnome-db2html3.c
