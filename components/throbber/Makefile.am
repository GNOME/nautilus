NULL = 

INCLUDES =						\
	-DG_LOG_DOMAIN=\"Nautilus-Throbber\"		\
	-DDATADIR=\""$(datadir)"\" 			\
	-I$(top_srcdir)					\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\"	\
	$(NAUTILUS_PRIVATE_CFLAGS)			\
	$(NULL)

bin_PROGRAMS =					\
	nautilus-throbber

nautilus_throbber_SOURCES =		        \
	nautilus-throbber.c			\
	nautilus-throbber.h			\
	main.c

nautilus_throbber_LDADD =			\
	$(top_builddir)/libnautilus/libnautilus.la	\
	$(top_builddir)/libnautilus-private/libnautilus-private.la \
	$(NAUTILUS_PRIVATE_LIBS) \
	$(NULL)

oafdir = $(datadir)/oaf
oaf_in_files =					\
	Nautilus_Control_throbber.oaf.in	\
	$(NULL)
oaf_DATA = $(oaf_in_files:.oaf.in=.oaf)

@XML_I18N_MERGE_OAF_RULE@

EXTRA_DIST = 			\
	$(oaf_in_files)		\
	$(oaf_DATA)		\
	$(NULL)
