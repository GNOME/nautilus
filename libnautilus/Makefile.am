include $(top_srcdir)/Makefile.shared

lib_LTLIBRARIES=libnautilus.la

INCLUDES=\
	-I$(builddir) \
	-I$(top_srcdir) \
	-I$(top_builddir) \
	$(NAUTILUS_DEPENDENCY_CFLAGS) \
	-DDATADIR=\""$(datadir)"\" \
	$(NULL)

libnautilus_la_LDFLAGS=\
	$(NULL)

nautilus_view_component_idl_sources =		\
	nautilus-view-component-stubs.c		\
	nautilus-view-component-skels.c		\
	nautilus-view-component.h		\
	nautilus-view-component-common.c

nautilus_distributed_undo_idl_sources =	\
	nautilus-distributed-undo-stubs.c	\
	nautilus-distributed-undo-skels.c	\
	nautilus-distributed-undo.h		\
	nautilus-distributed-undo-common.c

libnautilusincludedir=$(includedir)/libnautilus

libnautilusinclude_HEADERS=			\
	libnautilus.h				\
	nautilus-bonobo-ui.h			\
	nautilus-bonobo-workarounds.h		\
	nautilus-clipboard.h			\
	nautilus-distributed-undo.h		\
	nautilus-idle-queue.h			\
	nautilus-view-component.h		\
	nautilus-view.h				\
	nautilus-view-standard-main.h		\
	nautilus-undo.h				\
	nautilus-undo-private.h			\
	$(NULL)

libnautilus_la_SOURCES=					\
	$(nautilus_view_component_idl_sources)		\
	$(nautilus_distributed_undo_idl_sources)	\
	nautilus-bonobo-workarounds.c			\
	nautilus-clipboard.c				\
	nautilus-idle-queue.c				\
	nautilus-undo-transaction.c			\
	nautilus-undo-transaction.h			\
	nautilus-undo.c					\
	nautilus-view.c					\
	nautilus-view-standard-main.c			\
	$(NULL)

$(nautilus_view_component_idl_sources): nautilus_view_component_idl_stamp
nautilus_view_component_idl_stamp: nautilus-view-component.idl
	$(ORBIT_IDL) $(IDL_CFLAGS) --define=NAUTILUS_VIEW_COMPONENT_COMPILATION $(srcdir)/nautilus-view-component.idl
	touch nautilus_view_component_idl_stamp

$(nautilus_distributed_undo_idl_sources): nautilus_distributed_undo_idl_stamp
nautilus_distributed_undo_idl_stamp: nautilus-distributed-undo.idl
	$(ORBIT_IDL) $(IDL_CFLAGS) $(srcdir)/nautilus-distributed-undo.idl
	touch nautilus_distributed_undo_idl_stamp

$(libnautilus_la_OBJECTS): nautilus_view_component_idl_stamp nautilus_distributed_undo_idl_stamp

uidir = $(datadir)/gnome/ui
ui_DATA = \
	nautilus-clipboard-ui.xml	\
	$(NULL)

idldir = $(datadir)/idl
idl_DATA = \
	nautilus-view-component.idl	\
	nautilus-distributed-undo.idl	\
	$(NULL)

EXTRA_DIST = \
	$(ui_DATA)			\
	$(idl_DATA)			\
	$(NULL)

BUILT_SOURCES = \
	$(nautilus_view_component_idl_sources)		\
	$(nautilus_distributed_undo_idl_sources)	\
	$(NULL)

CLEANFILES = \
	$(nautilus_view_component_idl_sources)		\
	nautilus_view_component_idl_stamp		\
	$(nautilus_distributed_undo_idl_sources)	\
	nautilus_distributed_undo_idl_stamp             \
	$(NULL)

dist-hook:
	cd $(distdir); rm -f $(CLEANFILES)
