# FIXME should be a shared library component or compiled in.

bin_PROGRAMS = ntl-file-manager

INCLUDES=-I$(top_srcdir) -I$(top_builddir) \
	$(BONOBO_CFLAGS) $(GNORBA_CFLAGS) \
	$(GNOMEUI_CFLAGS) -DVERSION="\"$(VERSION)\"" \
	$(GDK_PIXBUF_CFLAGS)				\
	$(WERROR)

LDADD=	../../libnautilus/libnautilus.la \
	$(BONOBO_LIBS) \
	$(GNORBA_LIBS) \
	$(GNOMEUI_LIBS) \
	$(GDK_PIXBUF_LIBS) \
	$(VFS_LIBS) \
	$(XML_LIBS)

gnorbadir = $(sysconfdir)/CORBA/servers
gnorba_DATA =					\
	ntl-file-manager.goad

dfos_corba_srcs=					\
	GNOME_Desktop_FileOperationService.h		\
	GNOME_Desktop_FileOperationService-stubs.c	\
	GNOME_Desktop_FileOperationService-skels.c	\
	GNOME_Desktop_FileOperationService-common.c

ntl_file_manager_SOURCES=		\
	fm-icon-cache.c			\
	fm-icon-cache.h			\
	dfos-corba.c			\
	dfos-corba.h			\
	dfos-xfer-progress-dialog.c	\
	dfos-xfer-progress-dialog.h	\
	dfos-xfer.c			\
	dfos-xfer.h			\
	dfos.c				\
	dfos.h				\
	$(dfos_corba_srcs)		\
	fm-directory-view.h		\
	fm-directory-view.c		\
	fm-directory-view-icons.h	\
	fm-directory-view-icons.c	\
	fm-directory-view-list.h	\
	fm-directory-view-list.c	\
	fm-main.c

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
# 	fm-icon-cache.c			\
# 	fm-icon-cache.h			\
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
	orbit-idl $(top_srcdir)/idl/GNOME_Desktop_FileOperationService.idl
	touch dfos_corba_stamp

BUILT_SOURCES=$(dfos_corba_srcs)

CLEAN_FILES=$(dfos_corba_srcs) dfos_corba_stamp

EXTRA_DIST = ntl-file-manager.goad
