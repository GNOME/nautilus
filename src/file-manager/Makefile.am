include $(top_srcdir)/Makefile.shared

noinst_LTLIBRARIES=libnautilus-file-manager.la

INCLUDES = 					\
	-I$(top_srcdir) 			\
	-I$(top_srcdir)/cut-n-paste-code	\
	$(BASE_CFLAGS)				\
	$(COMMON_CFLAGS)			\
	$(NAUTILUS_CFLAGS)			\
	$(WARNING_CFLAGS)			\
	-DNAUTILUS_DATADIR=\""$(datadir)/nautilus"\" \
	-DDATADIR=\""$(datadir)"\"		\
	$(DISABLE_DEPRECATED_CFLAGS)            \
	$(NULL)	

BUILT_SOURCES = \
	fm-marshal.c \
	fm-marshal.h \
	$(NULL)

libnautilus_file_manager_la_SOURCES=	\
	fm-actions.h			\
	fm-desktop-icon-view.c		\
	fm-desktop-icon-view.h 		\
	fm-directory-view.c		\
	fm-directory-view.h 		\
	fm-ditem-page.c			\
	fm-ditem-page.h			\
	fm-error-reporting.c		\
	fm-error-reporting.h 		\
	fm-icon-container.c		\
	fm-icon-container.h		\
	fm-icon-view.c			\
	fm-icon-view.h 			\
	fm-list-model.c			\
	fm-list-model.h			\
	fm-list-view-private.h 		\
	fm-list-view.c			\
	fm-list-view.h 			\
	fm-properties-window.c		\
	fm-properties-window.h 		\
	nautilus-audio-mime-types.h	\
	$(NULL)

nodist_libnautilus_file_manager_la_SOURCES=\
	$(BUILT_SOURCES) \
	$(NULL)

fm-marshal.list: $(libnautilus_file_manager_la_SOURCES) Makefile.am
	$(AM_V_GEN)( cd $(srcdir) && \
	sed -n -e 's/.*fm_marshal_\([[:upper:][:digit:]]*__[[:upper:][:digit:]_]*\).*/\1/p' \
	$(libnautilus_file_manager_la_SOURCES) ) \
	| sed -e 's/__/:/' -e 'y/_/,/' | sort -u > $@.tmp
	@if cmp -s $@.tmp $@; then \
		rm $@.tmp; \
	else \
		mv $@.tmp $@; \
	fi

%-marshal.c: %-marshal.list Makefile
	$(AM_V_GEN)echo "#include \"fm-marshal.h\"" > $@ && \
	$(GLIB_GENMARSHAL) --body --prefix=$(subst -,_,$*)_marshal $< >> $*-marshal.c

%-marshal.h: %-marshal.list Makefile
	$(AM_V_GEN)$(GLIB_GENMARSHAL) --header --prefix=$(subst -,_,$*)_marshal $< > $*-marshal.h

EMPTY_VIEW_SOURCES = \
	fm-empty-view.c \
	fm-empty-view.h

if ENABLE_EMPTY_VIEW
libnautilus_file_manager_la_SOURCES += $(EMPTY_VIEW_SOURCES)
endif

uidir = $(datadir)/nautilus/ui
ui_DATA =					\
	nautilus-desktop-icon-view-ui.xml	\
	nautilus-directory-view-ui.xml		\
	nautilus-icon-view-ui.xml		\
	nautilus-list-view-ui.xml		\
	$(NULL)

EXTRA_DIST = \
	$(ui_DATA) \
	fm-marshal.list

CLEANFILES = \
	$(BUILT_SOURCES) \
	fm-marshal.list \
	$(NULL)

