include $(top_srcdir)/Makefile.shared

@XML_I18N_MERGE_DESKTOP_RULE@

DESKTOP_IN_FILES=nautilus.desktop.in nautilus-help.desktop.in
DESKTOP_FILES=$(DESKTOP_IN_FILES:.desktop.in=.desktop)

if HAVE_APPLETS
APPLETS_SUBDIRS = applets
endif

SUBDIRS =			\
	data 			\
	icons 			\
	cut-n-paste-code 	\
	libnautilus 		\
	libnautilus-adapter	\
	libnautilus-private 	\
	helper-utilities 	\
	helper-scripts		\
	src 			\
	components		\
	$(APPLETS_SUBDIRS)	\
	test 			\
	user-guide		\
	help			\
	po 			\
	omf-install		\
	$(NULL)

EXTRA_DIST=					\
	TRADEMARK_NOTICE			\
	COPYING.LIB				\
	COPYING-DOCS				\
	RPMs-README				\
	HACKING					\
	MAINTAINERS				\
	nautilus.spec.in			\
	nautilus-clean.sh			\
	nautilus.desktop			\
	nautilus.desktop.in			\
	nautilus-help.desktop			\
	nautilus-help.desktop.in		\
	nautilus-launch-icon.png		\
	gnome-starthere-96.png			\
	gnome-starthere.png			\
	nautilusConf.sh.in			\
	nautilus-config.in			\
	ChangeLog-20000414			\
	ChangeLog-20000625			\
	ChangeLog-20000831			\
	ChangeLog-20001018			\
	xml-i18n-extract.in			\
	xml-i18n-merge.in			\
	xml-i18n-update.in			\
	Makefile.shared				\
	$(DESKTOP_IN_FILES)			\
	$(DESKTOP_FILES)			\
	$(NULL)

appicondir = $(datadir)/pixmaps
appicon_DATA = 	nautilus-launch-icon.png 	\
		gnome-starthere-96.png		\
		gnome-starthere.png

Applicationsdir = $(datadir)/gnome/apps/Applications
Applications_DATA = $(DESKTOP_FILES)

Toplevelmenudir = $(datadir)/gnome/apps
Toplevelmenu_DATA = nautilus-help.desktop

## [The following is shamelessly copied from gnome-vfs.]

## We create `nautilusConf.sh' here and not from configure because we
## want to get the paths expanded correctly.  Macros like srcdir are
## given the value NONE in configure if the user doesn't specify them
## (this is an autoconf feature, not a bug).

bin_SCRIPTS = $(top_builddir)/nautilus-config

confexecdir=$(libdir)
confexec_DATA =					\
	nautilusConf.sh				\
	$(NULL)

nautilusConf.sh: nautilusConf.sh.in Makefile
	sed -e 's?\@NAUTILUS_LIBDIR\@?$(NAUTILUS_LIBDIR)?g' \
	    -e 's?\@NAUTILUS_INCLUDEDIR\@?$(NAUTILUS_INCLUDEDIR)?g' \
	    -e 's?\@VERSION\@?$(VERSION)?g' \
	    -e 's?\@NAUTILUS_LIBS\@?$(NAUTILUS_LIBS)?g' \
	      < $(srcdir)/nautilusConf.sh.in > nautilusConf.tmp \
	  && mv nautilusConf.tmp nautilusConf.sh

nautilus-config: nautilus-config.in Makefile
	sed -e 's?\@LIBDIR\@?$(libdir)?g' \
	    -e 's?\@NAUTIlUS_LIBDIR\@?$(NAUTILUS_LIBDIR)?g' \
	    -e 's?\@NAUTILUS_INCLUDEDIR\@?$(NAUTILUS_INCLUDEDIR)?g' \
	    -e 's?\@VERSION\@?$(VERSION)?g' \
	    -e 's?\@NAUTILUS_LIBS\@?$(NAUTILUS_LIBS)?g' \
	      < $(srcdir)/nautilus-config.in > nautilus-config.tmp \
	  && mv nautilus-config.tmp nautilus-config && \
		chmod 755 nautilus-config

##################################################################
#
# eazel-logos/*, if it exists
#
##################################################################

icondir = $(datadir)/pixmaps/nautilus/eazel-logos

if HAVE_EAZEL_LOGOS
EAZEL_LOGOS_ICONS = \
	$(top_srcdir)/eazel-logos/LICENSE \
	$(top_srcdir)/eazel-logos/About_Image.png \
	$(top_srcdir)/eazel-logos/about_background.png \
	$(top_srcdir)/eazel-logos/default.xml \
	$(top_srcdir)/eazel-logos/druid_header.png \
	$(top_srcdir)/eazel-logos/eazel-logo-right-side-logo.png \
	$(NULL)
endif

##################################################################
#
# eazel-logos/throbber/*, if it exists
#
##################################################################

throbberdir = $(datadir)/pixmaps/nautilus/eazel-logos/throbber

if HAVE_EAZEL_LOGOS
EAZEL_LOGOS_THROBBER = \
	$(top_srcdir)/eazel-logos/throbber/001.png \
	$(top_srcdir)/eazel-logos/throbber/002.png \
	$(top_srcdir)/eazel-logos/throbber/003.png \
	$(top_srcdir)/eazel-logos/throbber/004.png \
	$(top_srcdir)/eazel-logos/throbber/005.png \
	$(top_srcdir)/eazel-logos/throbber/006.png \
	$(top_srcdir)/eazel-logos/throbber/007.png \
	$(top_srcdir)/eazel-logos/throbber/008.png \
	$(top_srcdir)/eazel-logos/throbber/009.png \
	$(top_srcdir)/eazel-logos/throbber/010.png \
	$(top_srcdir)/eazel-logos/throbber/011.png \
	$(top_srcdir)/eazel-logos/throbber/012.png \
	$(top_srcdir)/eazel-logos/throbber/013.png \
	$(top_srcdir)/eazel-logos/throbber/014.png \
	$(top_srcdir)/eazel-logos/throbber/015.png \
	$(top_srcdir)/eazel-logos/throbber/rest.png \
	$(NULL)
endif

##################################################################
#
# dist-hook
#
##################################################################

dist-hook: nautilus.spec
	cp nautilus.spec $(distdir)

# This is ridiculous, but automake apparently won't allow conditionals
# in EXTRA_DIST.

	mkdir -p $(distdir)/eazel-logos
	list='$(EAZEL_LOGOS_ICONS)'; \
	for i in $$list; \
	do \
		cp $$i $(distdir)/eazel-logos; \
	done

	mkdir -p $(distdir)/eazel-logos/throbber
	list='$(EAZEL_LOGOS_THROBBER)'; \
	for i in $$list; \
	do \
		cp $$i $(distdir)/eazel-logos/throbber; \
	done

##################################################################
#
# install-data-hook
#
##################################################################

install-data-hook: nautilus-clean.sh
	$(mkinstalldirs) $(DESTDIR)/$(prefix)/bin
	@list='nautilus-clean.sh'; for p in $$list; do\
	  if test -f $(srcdir)/$$p; then \
	    echo "  $(INSTALL_PROGRAM) $(srcdir)/$$p $(DESTDIR)/$(prefix)/bin/$$p"; \
	    $(INSTALL_PROGRAM) $(srcdir)/$$p $(DESTDIR)/$(prefix)/bin/$$p; \
	  else :; fi; \
	done

# This is slightly less ridiculous.  I'm doing it this way instead of with
# an automake DATA rule because it's easier to deal with the relative paths
# like this.

	$(mkinstalldirs) $(icondir)
	@list='$(EAZEL_LOGOS_ICONS)'; \
	for i in $$list; \
	do \
		echo $(INSTALL_PROGRAM) $$i $(icondir)/`basename $$i`; \
		$(INSTALL_PROGRAM) $$i $(icondir)/`basename $$i`; \
	done

	$(mkinstalldirs) $(throbberdir)
	@list='$(EAZEL_LOGOS_THROBBER)'; \
	for i in $$list; \
	do \
		echo $(INSTALL_PROGRAM) $$i $(throbberdir)/`basename $$i`; \
		$(INSTALL_PROGRAM) $$i $(throbberdir)/`basename $$i`; \
	done
