NULL = 

PAM_FILES = eazel-helper.security eazel-helper.pam

bin_PROGRAMS = eazel-helper


# do mv on a separate step in case the sed fails
eazel-helper.security: eazel-helper.security.in Makefile
	sed -e 's^\@bindir\@^$(bindir)^g' < eazel-helper.security.in > eazel-helper.security.tmp \
		&& mv eazel-helper.security.tmp eazel-helper.security

install-data-local: $(PAM_FILES)
	mkdir -p $(sysconfdir)/security/console.apps
	cp eazel-helper.security $(sysconfdir)/security/console.apps/eazel-helper
	mkdir -p $(sysconfdir)/pam.d
	cp eazel-helper.pam $(sysconfdir)/pam.d/eazel-helper

eazel_helper_SOURCES = \
	eazel-helper.c		\
	$(NULL)

eazel_helper_LDADD = \
	$(GLIB_LIBS)		\
	$(NULL)

INCLUDES = \
	$(GLIB_CFLAGS)		\
	$(WERROR)		\
	$(NULL)

