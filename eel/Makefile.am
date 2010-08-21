NULL=

noinst_LTLIBRARIES=libeel-2.la

INCLUDES =						\
	-DG_LOG_DOMAIN=\"Eel\"				\
	-I$(top_srcdir)					\
	$(CORE_CFLAGS)					\
	$(WARNING_CFLAGS)				\
	-DDATADIR=\""$(datadir)"\"			\
	-DSOURCE_DATADIR=\""$(top_srcdir)/data"\"	\
	-DGNOMELOCALEDIR=\""$(prefix)/${DATADIRNAME}/locale"\"	\
	-DG_DISABLE_DEPRECATED				\
	-DGDK_DISABLE_DEPRECATED			\
	-DGDK_PIXBUF_DISABLE_DEPRECATED			\
	-DGMENU_I_KNOW_THIS_IS_UNSTABLE			\
	$(NULL)

BUILT_SOURCES = \
	eel-marshal.c \
	eel-marshal.h \
	$(NULL)

libeel_2_la_LDFLAGS =				\
	-no-undefined				\
	$(NULL)

libeel_2_la_LIBADD = 				\
	$(CORE_LIBS)				\
	$(RENDER_LIBS)				\
	$(X_LIBS)				\
	$(NULL)

libeel_2_la_SOURCES =				\
	eel-accessibility.c			\
	eel-art-extensions.c			\
	eel-background.c			\
	eel-canvas.c				\
	eel-canvas-util.c			\
	eel-canvas-rect-ellipse.c		\
	eel-debug.c				\
	eel-editable-label.c			\
	eel-gdk-extensions.c			\
	eel-gdk-pixbuf-extensions.c		\
	eel-glib-extensions.c			\
	eel-gnome-extensions.c			\
	eel-graphic-effects.c			\
	eel-gtk-extensions.c			\
	eel-lib-self-check-functions.c		\
	eel-self-checks.c			\
	eel-stock-dialogs.c			\
	eel-string.c				\
	eel-vfs-extensions.c			\
	eel-lib-self-check-functions.h		\
	$(NULL)

eel_headers =					\
	eel-accessibility.h			\
	eel-art-extensions.h			\
	eel-background.h			\
	eel-canvas.h				\
	eel-canvas-util.h			\
	eel-canvas-rect-ellipse.h		\
	eel-debug.h				\
	eel-editable-label.h			\
	eel-gdk-extensions.h			\
	eel-gdk-pixbuf-extensions.h		\
	eel-glib-extensions.h			\
	eel-gnome-extensions.h			\
	eel-graphic-effects.h			\
	eel-gtk-extensions.h			\
	eel-gtk-macros.h			\
	eel-self-checks.h			\
	eel-stock-dialogs.h			\
	eel-string.h				\
	eel-vfs-extensions.h			\
	eel.h					\
	$(NULL)

nodist_libeel_2_la_SOURCES =			\
	$(BUILT_SOURCES)			\
	$(NULL)

eel-marshal.list: $(libeel_2_la_SOURCES) Makefile.am
	$(AM_V_GEN)( cd $(srcdir) && \
	sed -n -e 's/.*eel_marshal_\([[:upper:][:digit:]]*__[[:upper:][:digit:]_]*\).*/\1/p' \
	$(libeel_2_la_SOURCES) ) \
	| sed -e 's/__/:/' -e 'y/_/,/' | sort -u > $@.tmp
	@if cmp -s $@.tmp $@; then \
		rm $@.tmp; \
	else \
		mv $@.tmp $@; \
	fi

%-marshal.c: %-marshal.list Makefile
	$(AM_V_GEN)echo "#include \"eel-marshal.h\"" > $@ && \
	$(GLIB_GENMARSHAL) --body --prefix=$(subst -,_,$*)_marshal $< >> $*-marshal.c

%-marshal.h: %-marshal.list Makefile
	$(AM_V_GEN)$(GLIB_GENMARSHAL) --header --prefix=$(subst -,_,$*)_marshal $< > $*-marshal.h

noinst_PROGRAMS = check-program

check_program_SOURCES = check-program.c
check_program_DEPENDENCIES = libeel-2.la
check_program_LDADD = $(EEL_LIBS)
check_program_LDFLAGS =	$(check_program_DEPENDENCIES) -lm

TESTS = check-eel

EXTRA_DIST =					\
	$(eel_headers)				\
	check-eel				\
	eel-marshal.list			\
	$(NULL)

CLEANFILES = \
	$(BUILT_SOURCES) \
	$(NULL)
