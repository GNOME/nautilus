NULL=

noinst_LTLIBRARIES=libeel-2.la

INCLUDES =						\
	-DG_LOG_DOMAIN=\"Eel\"				\
	-I$(top_srcdir)					\
	$(CORE_CFLAGS)					\
	-DDATADIR=\""$(datadir)"\"			\
	-DSOURCE_DATADIR=\""$(top_srcdir)/data"\"	\
	-DGNOMELOCALEDIR=\""$(prefix)/${DATADIRNAME}/locale"\"	\
	-DG_DISABLE_DEPRECATED				\
	-DGDK_DISABLE_DEPRECATED			\
	-DGDK_PIXBUF_DISABLE_DEPRECATED			\
	-DGMENU_I_KNOW_THIS_IS_UNSTABLE			\
	$(NULL)

libeel_2_la_LDFLAGS =				\
	-no-undefined				\
	$(CORE_LIBS)				\
	$(RENDER_LIBS)				\
	$(X_LIBS)				\
	$(NULL)

libeel_2_la_SOURCES =				\
	eel-accessibility.c			\
	eel-alert-dialog.c			\
	eel-art-extensions.c			\
	eel-art-gtk-extensions.c		\
	eel-background.c			\
	eel-background-box.c                    \
	eel-canvas.c				\
	eel-canvas-util.c			\
	eel-canvas-rect-ellipse.c		\
	eel-debug-drawing.c			\
	eel-debug.c				\
	eel-editable-label.c			\
	eel-enumeration.c			\
	eel-gconf-extensions.c			\
	eel-gdk-extensions.c			\
	eel-gdk-pixbuf-extensions.c		\
	eel-glib-extensions.c			\
	eel-gnome-extensions.c			\
	eel-graphic-effects.c			\
	eel-gtk-container.c			\
	eel-gtk-extensions.c			\
	eel-i18n.c				\
	eel-image-table.c			\
	eel-labeled-image.c			\
	eel-lib-self-check-functions.c		\
	eel-pango-extensions.c			\
	eel-preferences-builder.c		\
	eel-preferences.c			\
	eel-self-checks.c			\
	eel-stock-dialogs.c			\
	eel-string.c				\
	eel-types.c				\
	eel-vfs-extensions.c			\
	eel-wrap-table.c			\
	eel-xml-extensions.c			\
	eel-lib-self-check-functions.h		\
	$(NULL)

eel_headers =					\
	eel-accessibility.h			\
	eel-alert-dialog.h			\
	eel-art-extensions.h			\
	eel-art-gtk-extensions.h		\
	eel-background.h			\
	eel-background-box.h                    \
	eel-canvas.h				\
	eel-canvas-util.h			\
	eel-canvas-rect-ellipse.h		\
	eel-debug-drawing.h			\
	eel-debug.h				\
	eel-editable-label.h			\
	eel-enumeration.h			\
	eel-gconf-extensions.h			\
	eel-gdk-extensions.h			\
	eel-gdk-pixbuf-extensions.h		\
	eel-glib-extensions.h			\
	eel-gnome-extensions.h			\
	eel-graphic-effects.h			\
	eel-gtk-container.h			\
	eel-gtk-extensions.h			\
	eel-gtk-macros.h			\
	eel-i18n.h				\
	eel-image-table.h			\
	eel-labeled-image.h			\
	eel-pango-extensions.h			\
	eel-preferences.h			\
	eel-self-checks.h			\
	eel-stock-dialogs.h			\
	eel-string.h				\
	eel-types.h				\
	eel-vfs-extensions.h			\
	eel-wrap-table.h			\
	eel-xml-extensions.h			\
	eel.h					\
	$(NULL)

marshal_sources = 				\
	eel-marshal.h				\
	eel-marshal.c				\
	$(NULL)

eel-marshal.h: eelmarshal.list $(GLIB_GENMARSHAL)
	$(GLIB_GENMARSHAL) $< --header --prefix=eel_marshal > $@
eel-marshal.c: eelmarshal.list $(GLIB_GENMARSHAL)
	$(GLIB_GENMARSHAL) $< --body --prefix=eel_marshal > $@

stamp_sources =					\
	eel-enums.defs				\
	eel-type-builtins-evals.c		\
	$(NULL)

stamps =					\
	eel-makeenums-stamp			\
	eel-stamp				\
	$(NULL)

eel-makeenums-stamp: makeenums.pl $(eel_headers)
	$(PERL) $< defs $(filter-out $<,$^) > xgen-eed \
	&& (cmp -s xgen-eed eel-enums.defs || mv -f xgen-eed eel-enums.defs) \
	&& rm -f xgen-eed \
	&& $(PERL) $< arrays $(filter-out $<,$^) > xgen-etbe \
	&& (cmp -s xgen-etbe eel-type-builtins-evals.c || mv -f xgen-etbe eel-type-builtins-evals.c) \
	&& rm -f xgen-etbe \
	&& echo timestamp > $@

maketypes_sources =				\
	eel-type-builtins.h			\
	eel-type-builtins-ids.c			\
	eel-type-builtins-vars.c		\
	$(NULL)

eel-stamp: eel-makeenums-stamp $(maketypes_sources)
	echo timestamp > $@

eel-type-builtins.h: eel-enums.defs maketypes.awk eel-makeenums-stamp
	LC_ALL=C $(AWK) -f $(srcdir)/maketypes.awk $< macros > $@
eel-type-builtins-vars.c: eel-enums.defs maketypes.awk eel-makeenums-stamp
	LC_ALL=C $(AWK) -f $(srcdir)/maketypes.awk $< variables > $@
eel-type-builtins-ids.c: eel-enums.defs maketypes.awk eel-makeenums-stamp
	LC_ALL=C $(AWK) -f $(srcdir)/maketypes.awk $< entries > $@

noinst_PROGRAMS = check-program

check_program_SOURCES = check-program.c
check_program_DEPENDENCIES = libeel-2.la
check_program_LDADD = $(EEL_LIBS)
check_program_LDFLAGS =	$(check_program_DEPENDENCIES) -lm

TESTS = check-eel

EXTRA_DIST =					\
	$(eel_headers)				\
	eel-type-builtins.h			\
	eel-marshal.h				\
	check-eel				\
	eelmarshal.list				\
	makeenums.pl				\
	maketypes.awk				\
	$(NULL)

$(libeel_2_la_OBJECTS): $(marshal_sources)

# This trick causes the stamp file to be built first.
Makefile: eel-stamp

# This trick causes the generated files to be built the first time.
$(stamp_sources): # never add any dependencies
	test -f $@ || touch $@

built_sources =	$(stamps) $(stamp_sources) $(maketypes_sources) $(marshal_sources)
CLEANFILES = $(built_sources)
DONT_DIST_FILES = $(built_sources)

dist-hook:
	for file in $(DONT_DIST_FILES) ; do \
	    rm -f $(distdir)/$$file ; \
	done
