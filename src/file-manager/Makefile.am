noinst_LTLIBRARIES=libntl-file-manager.la

INCLUDES=-I$(top_srcdir) -I$(top_builddir) \
	$(BONOBO_CFLAGS) $(OAF_CFLAGS) \
	$(GNOMEUI_CFLAGS) -DVERSION="\"$(VERSION)\"" \
	$(GNOMECANVASPIXBUF_INCLUDEDIR)			\
	$(WERROR)					\
	-D_REENTRANT

dfos_corba_srcs=					\
	GNOME_Desktop_FileOperationService.h		\
	GNOME_Desktop_FileOperationService-stubs.c	\
	GNOME_Desktop_FileOperationService-skels.c	\
	GNOME_Desktop_FileOperationService-common.c

libntl_file_manager_la_SOURCES=	\
	$(dfos_corba_srcs)		\
	dfos-corba.c			\
	dfos-corba.h			\
	dfos-xfer-progress-dialog.c	\
	dfos-xfer-progress-dialog.h	\
	dfos-xfer.c			\
	dfos-xfer.h			\
	dfos.c				\
	dfos.h				\
	fm-directory-view.c		\
	fm-directory-view.h		\
	fm-error-reporting.c		\
	fm-error-reporting.h		\
	fm-icon-text-window.c		\
	fm-icon-text-window.h		\
	fm-icon-view.c			\
	fm-icon-view.h			\
	fm-list-view.c			\
	fm-list-view.h			\
	fm-properties-window.c		\
	fm-properties-window.h		

dfos-corba.h: GNOME_Desktop_FileOperationService.h

$(dfos_corba_srcs): dfos_corba_stamp
dfos_corba_stamp: $(top_srcdir)/idl/GNOME_Desktop_FileOperationService.idl
	$(prefix)/bin/orbit-idl -I${datadir}/idl $(top_srcdir)/idl/GNOME_Desktop_FileOperationService.idl
	touch dfos_corba_stamp

BUILT_SOURCES=$(dfos_corba_srcs)

CLEANFILES=$(dfos_corba_srcs) dfos_corba_stamp
