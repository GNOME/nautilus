noinst_LTLIBRARIES=libntl-file-manager.la

INCLUDES=-I$(top_srcdir) -I$(top_builddir) \
	$(BONOBO_CFLAGS) $(GNORBA_CFLAGS) \
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
	fm-directory-view-icons.c	\
	fm-directory-view-icons.h	\
	fm-directory-view-list.c	\
	fm-directory-view-list.h	\
	fm-directory-view.c		\
	fm-directory-view.h		\
	fm-icon-text-window.c		\
	fm-icon-text-window.h		\
	fm-icons-controller.c		\
	fm-icons-controller.h		\
	fm-properties-window.c		\
	fm-properties-window.h		\
        fm-signaller.c 			\
        fm-signaller.h 		

# noinst_PROGRAMS=gnome-desktop

# gnome_desktop_SOURCES=			\
# 	desktop-canvas.c		\
# 	desktop-canvas.h		\
# 	desktop-item.c			\
# 	desktop-item.h			\
# 	desktop-layout.h		\
# 	desktop-layout.c		\
# 	desktop-menu.h			\
# 	desktop-menu.c			\
# 	desktop-window.c		\
# 	desktop-window.h		\
# 	fm-directory-list.h		\
# 	fm-directory-list.c		\
# 	gnome-desktop.c

# gnome_desktop_LDADD=\
# 		$(BONOBO_LIBS) \
# 		$(GNORBA_LIBS) \
# 		$(GNOMEUI_LIBS) \
# 		$(GDK_PIXBUF_LIBS) \
# 		$(VFS_LIBS)

dfos-corba.h: GNOME_Desktop_FileOperationService.h

$(dfos_corba_srcs): dfos_corba_stamp

dfos_corba_stamp: $(top_srcdir)/idl/GNOME_Desktop_FileOperationService.idl
	orbit-idl `gnome-config --cflags idl` $(top_srcdir)/idl/GNOME_Desktop_FileOperationService.idl
	touch dfos_corba_stamp

BUILT_SOURCES=$(dfos_corba_srcs)

CLEAN_FILES=$(dfos_corba_srcs) dfos_corba_stamp
