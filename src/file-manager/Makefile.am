include $(top_srcdir)/Makefile.shared

noinst_LTLIBRARIES=libnautilus-file-manager.la

INCLUDES = 					\
	-I$(top_srcdir) 			\
	-I$(top_builddir) 			\
	-I$(top_srcdir)/cut-n-paste-code 	\
	$(BONOBO_CFLAGS) 			\
	$(MEDUSA_CFLAGS)			\
	$(OAF_CFLAGS) 				\
	$(GNOMEUI_CFLAGS) 			\
	$(VFS_CFLAGS) 				\
	$(EEL_CFLAGS)				\
	-DVERSION="\"$(VERSION)\"" 		\
	-D_REENTRANT 				\
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
	$(NULL)

noinst_HEADERS = 			\
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
