include $(top_srcdir)/Makefile.shared

noinst_LTLIBRARIES=libnautilus-file-manager.la

INCLUDES = 					\
	-I$(top_srcdir) 			\
	$(CORE_CFLAGS)				\
	-DVERSION="\"$(VERSION)\""		\
	-DNAUTILUS_DATADIR=\""$(datadir)/nautilus"\" \
	-DDATADIR=\""$(datadir)"\"		\
	-DG_DISABLE_DEPRECATED \
	-DGDK_DISABLE_DEPRECATED \
	-DGDK_PIXBUF_DISABLE_DEPRECATED \
	-DGTK_DISABLE_DEPRECATED \
	-DGNOME_DISABLE_DEPRECATED \
	$(NULL)	

EGG_RECENT_FILES =		\
	egg-recent-model.c	\
	egg-recent-model.h	\
	egg-recent-item.c	\
	egg-recent-item.h       \
	egg-recent-vfs-utils.c  \
	egg-recent-vfs-utils.h

libnautilus_file_manager_la_SOURCES=	\
	fm-desktop-icon-view.c		\
	fm-directory-view.c		\
	fm-error-reporting.c		\
	fm-icon-container.c		\
	fm-icon-view.c			\
	fm-list-model.c			\
	fm-list-view.c			\
	fm-properties-window.c		\
	fm-search-list-view.c		\
	nautilus-indexing-info.c	\
	fm-desktop-icon-view.h 		\
	fm-directory-view.h 		\
	fm-error-reporting.h 		\
	fm-icon-container.h		\
	fm-icon-view.h 			\
	fm-list-model.h			\
	fm-list-view-private.h 		\
	fm-list-view.h 			\
	fm-properties-window.h 		\
	fm-search-list-view.h 		\
	nautilus-indexing-info.h 	\
	$(EGG_RECENT_FILES)		\
	$(NULL)

uidir = $(datadir)/gnome-2.0/ui
ui_DATA =					\
	nautilus-desktop-icon-view-ui.xml	\
	nautilus-directory-view-ui.xml		\
	nautilus-icon-view-ui.xml		\
	nautilus-search-list-view-ui.xml	\
	$(NULL)

EXTRA_DIST = $(ui_DATA)

EGG_DIR = $(srcdir)/../../../libegg/libegg/recent-files

regenerate-built-sources:
	EGGFILES="$(EGG_RECENT_FILES)" EGGDIR="$(EGG_DIR)" $(srcdir)/update-from-egg.sh
