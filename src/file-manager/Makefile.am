NULL = 

noinst_LTLIBRARIES=libnautilus-file-manager.la

INCLUDES = \
	-I$(top_srcdir) \
	-I$(top_builddir) \
	$(BONOBO_CFLAGS) \
	$(OAF_CFLAGS) \
	$(GNOMEUI_CFLAGS) \
	$(VFS_CFLAGS) \
	-DVERSION="\"$(VERSION)\"" \
	-D_REENTRANT \
	-DNAUTILUS_DATADIR=\""$(datadir)/nautilus"\" \
	$(NULL)	

dfos_corba_srcs=					\
	GNOME_Desktop_FileOperationService.h		\
	GNOME_Desktop_FileOperationService-stubs.c	\
	GNOME_Desktop_FileOperationService-skels.c	\
	GNOME_Desktop_FileOperationService-common.c \
	$(NULL)

libnautilus_file_manager_la_SOURCES=	\
	$(dfos_corba_srcs)		\
	dfos-corba.c			\
	dfos-xfer-progress-dialog.c	\
	dfos-xfer.c			\
	dfos.c				\
	fm-desktop-icon-view.c		\
	fm-directory-view.c		\
	fm-error-reporting.c		\
	fm-icon-text-window.c		\
	fm-icon-view.c			\
	fm-list-view.c			\
	fm-properties-window.c		\
	fm-search-list-view.c		\
	nautilus-trash-monitor.c	\
	$(NULL)

noinst_HEADERS = \
	dfos-corba.h \
	dfos-xfer-progress-dialog.h \
	dfos-xfer.h \
	dfos.h \
	fm-desktop-icon-view.h \
	fm-directory-view.h \
	fm-error-reporting.h \
	fm-icon-text-window.h \
	fm-icon-view.h \
	fm-list-view-private.h \
	fm-list-view.h \
	fm-properties-window.h \
	fm-search-list-view.h \
	nautilus-trash-monitor.h \
	$(NULL)

dfos-corba.h: GNOME_Desktop_FileOperationService.h

$(dfos_corba_srcs): dfos_corba_stamp
dfos_corba_stamp: $(top_srcdir)/idl/GNOME_Desktop_FileOperationService.idl
	$(ORBIT_IDL) $(IDL_CFLAGS) $(top_srcdir)/idl/GNOME_Desktop_FileOperationService.idl
	touch dfos_corba_stamp

BUILT_SOURCES=$(dfos_corba_srcs)

CLEANFILES=$(dfos_corba_srcs) dfos_corba_stamp
