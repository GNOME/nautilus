NULL = 

PAM_FILES = eazel-helper.security eazel-helper.pam

bin_PROGRAMS = eazel-helper


# do mv on a separate step in case the sed fails
eazel-helper.security: $(srcdir)/eazel-helper.security.in Makefile
	sed -e 's^\@bindir\@^$(bindir)^g' < $(srcdir)/eazel-helper.security.in > $(srcdir)/eazel-helper.security.tmp \
		&& mv $(srcdir)/eazel-helper.security.tmp $(srcdir)/eazel-helper.security

install-data-local: $(PAM_FILES)
	mkdir -p $(sysconfdir)/security/console.apps
	cp $(srcdir)/eazel-helper.security $(sysconfdir)/security/console.apps/eazel-helper
	mkdir -p $(sysconfdir)/pam.d
	cp $(srcdir)/eazel-helper.pam $(sysconfdir)/pam.d/eazel-helper

eazel_helper_SOURCES = \
	eazel-helper.c		\
	$(NULL)

eazel_helper_LDADD = \
	$(GLIB_LIBS)		\
	$(NULL)

INCLUDES = \
	$(GLIB_CFLAGS)		\
	$(NULL)

EXTRA_DIST = \
	eazel-helper.security.in \
	eazel-helper.pam \
	$(NULL)
