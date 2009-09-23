include $(top_srcdir)/Makefile.shared

@INTLTOOL_DESKTOP_RULE@

desktopdir       = $(datadir)/applications
desktop_DATA     = $(desktop_in_files:.desktop.in=.desktop)
desktop_in_files = 					\
	nautilus.desktop.in				\
	nautilus-home.desktop.in			\
	nautilus-computer.desktop.in			\
	nautilus-folder-handler.desktop.in		\
	nautilus-file-management-properties.desktop.in  \
	nautilus-browser.desktop.in                     \
	nautilus-autorun-software.desktop.in

SUBDIRS =			\
	eel			\
	libnautilus-extension	\
	cut-n-paste-code 	\
	libnautilus-private 	\
	src 			\
	test 			\
	po 			\
	data 			\
	icons 			\
	docs			\
	$(NULL)

EXTRA_DIST=					\
	COPYING.LIB				\
	COPYING-DOCS				\
	HACKING					\
	MAINTAINERS				\
	README.commits				\
	nautilus.desktop			\
	nautilus.desktop.in			\
	ChangeLog-20000414			\
	ChangeLog-20000625			\
	ChangeLog-20000831			\
	ChangeLog-20001018			\
	ChangeLog-20010201			\
	ChangeLog-20010420			\
	ChangeLog-20041029			\
	ChangeLog-20090417			\
	intltool-extract.in			\
	intltool-merge.in			\
	intltool-update.in			\
	Makefile.shared				\
	add-include-prefix			\
	$(desktop_in_files)			\
	$(NULL)

CLEANFILES = $(desktop_DATA)

DISTCLEANFILES = \
  intltool-extract \
  intltool-merge \
  intltool-update

DISTCHECK_CONFIGURE_FLAGS = --disable-update-mimedb --enable-gtk-doc

distclean-local:
	if test $(srdcir) = .; then :; else \
		rm -f ChangeLog; \
	fi

ChangeLog:
	@echo Creating $@
	@if test -d "$(srcdir)/.git"; then \
	  (GIT_DIR=$(top_srcdir)/.git ./missing --run git log 2.26.3.. --stat -M -C --name-status  --date=short --no-color) | fmt --split-only > $@.tmp \
	  && mv -f $@.tmp $@ \
	  || ($(RM) $@.tmp; \
	      echo Failed to generate ChangeLog, your ChangeLog may be outdated >&2; \
	      (test -f $@ || echo git-log is required to generate this file >> $@)); \
	else \
	  test -f $@ || \
	  (echo A git checkout and git-log is required to generate ChangeLog >&2 && \
	  echo A git checkout and git-log is required to generate this file >> $@); \
	fi

.PHONY: ChangeLog
