include $(top_srcdir)/Makefile.shared

noinst_LTLIBRARIES=libnautilus-file-manager.la

INCLUDES = 					\
	-I$(top_srcdir) 			\
	$(NAUTILUS_PRIVATE_CFLAGS)		\
	$(MEDUSA_CFLAGS)			\
	-DVERSION="\"$(VERSION)\"" 		\
	-DNAUTILUS_DATADIR=\""$(datadir)/nautilus"\" \
	-DDATADIR=\""$(datadir)"\"		\
	$(NULL)	


libnautilus_file_manager_la_SOURCES=	\
	fm-desktop-icon-view.c		\
	fm-directory-view.c		\
	fm-error-reporting.c		\
	fm-icon-view.c			\
	fm-list-view.c			\
	fm-properties-window.c		\
	fm-search-list-view.c		\
	nautilus-indexing-info.c	\
	fm-desktop-icon-view.h 		\
	fm-directory-view.h 		\
	fm-error-reporting.h 		\
	fm-icon-view.h 			\
	fm-list-view-private.h 		\
	fm-list-view.h 			\
	fm-properties-window.h 		\
	fm-search-list-view.h 		\
	nautilus-indexing-info.h 	\
	$(NULL)

uidir = $(datadir)/gnome/ui
ui_DATA =					\
	nautilus-desktop-icon-view-ui.xml	\
	nautilus-directory-view-ui.xml		\
	nautilus-icon-view-ui.xml		\
	nautilus-search-list-view-ui.xml	\
	$(NULL)

EXTRA_DIST = $(ui_DATA)
