helpdir = $(datadir)/gnome/help/$(docname)/$(lang)

#Scrollkeeper related stuff
omf_dir=$(top_srcdir)/omf-install

EXTRA_DIST = $(htmls) $(omffiles) $(figs)

all: omf

omf: $(omffiles)
	-for omffile in $(omffiles); do \
	  scrollkeeper-preinstall $(DESTDIR)$(helpdir)/index.html $$omffile $(omf_dir)/$$omffile; \
	done

app-dist-hook:
	-$(mkinstalldirs) $(distdir)/figures
	-cp $(srcdir)/$(docname)/*.html $(distdir)/$(docname)
	-cp $(srcdir)/$(docname)/*.png $(distdir)/$(docname)
	-cp $(srcdir)/$(docname)/*.css $(distdir)/$(docname)
	-cp $(srcdir)/figures/*.png $(distdir)/figures

install-data-am: omf
	-$(mkinstalldirs) $(DESTDIR)$(helpdir)/figures
	-for file in $(srcdir)/*.html; do \
	  basefile=`echo $$file | sed -e 's,^.*/,,'`; \
	  $(INSTALL_DATA) $$file $(DESTDIR)$(helpdir)/$$basefile; \
	done
	-for file in $(srcdir)/figures/*.png; do \
	  basefile=`echo $$file | sed -e  's,^.*/,,'`; \
	  $(INSTALL_DATA) $$file $(DESTDIR)$(helpdir)/figures/$$basefile; \
	done


