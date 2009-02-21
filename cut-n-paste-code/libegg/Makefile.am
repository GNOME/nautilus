NULL=

noinst_LTLIBRARIES = libegg.la

INCLUDES = $(LIBEGG_CFLAGS)

EGG_TREE_DND_FILES = 		\
	eggtreemultidnd.c	\
	eggtreemultidnd.h	\
	$(NULL)

EGG_SMCLIENT_FILES = 		\
	eggdesktopfile.c	\
	eggdesktopfile.h	\
	eggsmclient.c		\
	eggsmclient.h		\
	eggsmclient-private.h	\
	eggsmclient-xsmp.c	\
	$(NULL)

libegg_la_SOURCES = 		\
	$(EGG_TREE_DND_FILES)	\
	$(EGG_SMCLIENT_FILES)	\
	$(NULL)

libegg_la_CFLAGS =				\
	-DEGG_SM_CLIENT_BACKEND_XSMP		\
	-DG_LOG_DOMAIN=\""EggSMClient"\"	\
	$(LIBEGG_CFLAGS)			\
	$(WARNING_CFLAGS)			\
	$(DISABLE_DEPRECATED)

libegg_la_LIBADD = 	\
	$(LIBEGG_LIBS)	\
	-lSM -lICE

EXTRA_DIST = 			\
	update-from-egg.sh	\
	$(NULL)

EGG_TREE_DND_DIR = $(srcdir)/../../../libegg/libegg/treeviewutils
EGG_SMCLIENT_DIR = $(srcdir)/../../../libegg/libegg/smclient

regenerate-built-sources:
	EGGFILES="$(EGG_TREE_DND_FILES)" EGGDIR="$(EGG_TREE_DND_DIR)" $(srcdir)/update-from-egg.sh
	EGGFILES="$(EGG_SMCLIENT_FILES)" EGGDIR="$(EGG_SMCLIENT_DIR)" $(srcdir)/update-from-egg.sh
