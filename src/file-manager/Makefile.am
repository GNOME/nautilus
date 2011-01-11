include $(top_srcdir)/Makefile.shared

noinst_LTLIBRARIES=libnautilus-file-manager.la

INCLUDES = 					\
	-I$(top_srcdir) 			\
	-I$(top_srcdir)/cut-n-paste-code	\
	-I$(top_srcdir)/src			\
	$(BASE_CFLAGS)				\
	$(COMMON_CFLAGS)			\
	$(NAUTILUS_CFLAGS)			\
	$(WARNING_CFLAGS)			\
	-DNAUTILUS_DATADIR=\""$(datadir)/nautilus"\" \
	-DDATADIR=\""$(datadir)"\"		\
	$(DISABLE_DEPRECATED_CFLAGS)            \
	$(NULL)	

libnautilus_file_manager_la_SOURCES = $(NULL)

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
	$(ui_DATA)
