NULL=

noinst_LTLIBRARIES = libegg.la

INCLUDES = $(LIBEGG_CFLAGS)

EGG_SCREEN_FILES =			\
	egg-screen-exec.h	\
	egg-screen-exec.c	\
	egg-screen-url.h	\
	egg-screen-url.c	\
	egg-screen-help.h	\
	egg-screen-help.c	\
	$(NULL)

EGG_RECENT_FILES =		\
	egg-recent-model.c	\
	egg-recent-model.h	\
	egg-recent-item.c	\
	egg-recent-item.h       \
	$(NULL)

EGG_TREE_DND_FILES = 		\
	eggtreemultidnd.c	\
	eggtreemultidnd.h	\
	$(NULL)

libegg_la_SOURCES = 		\
	$(EGG_SCREEN_FILES)	\
	$(EGG_RECENT_FILES)	\
	$(EGG_TREE_DND_FILES)	\
	$(NULL)

EXTRA_DIST = 			\
	update-from-egg.sh	\
	$(NULL)

EGG_SCREEN_DIR = $(srcdir)/../../../libegg/libegg/screen-exec
EGG_RECENT_DIR = $(srcdir)/../../../libegg/libegg/recent-files
EGG_TREE_DND_DIR = $(srcdir)/../../../libegg/libegg/treeviewutils

regenerate-built-sources:
	EGGFILES="$(EGG_SCREEN_FILES)" EGGDIR="$(EGG_SCREEN_DIR)" $(srcdir)/update-from-egg.sh
	EGGFILES="$(EGG_RECENT_FILES)" EGGDIR="$(EGG_RECENT_DIR)" $(srcdir)/update-from-egg.sh
	EGGFILES="$(EGG_TREE_DND_FILES)" EGGDIR="$(EGG_TREE_DND_DIR)" $(srcdir)/update-from-egg.sh
