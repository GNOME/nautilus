SUBDIRS= docbook gnome-customization

INCLUDES = \
        -DGNOMELOCALEDIR=\""$(datadir)/locale"\"        \
	-DPREFIXDIR=\""$(prefix)"\" 			\
	$(GNOME_DB2HTML3_FLAG)

LDADD = 

bin_PROGRAMS = gnome-db2html3

gnome_db2html3_SOURCES = \
	gnome-db2html3.c
