helpdir = $(datadir)/gnome/help/$(docname)/$(lang)
help_DATA = \
	index.html

#Scrollkeeper related stuff
omf_dir=$(top_srcdir)/omf-install

EXTRA_DIST = $(docname).sgml $(help_DATA) $(omffiles) $(figs)

CLEANFILES = omf_timestamp

all: index.html omf

omf: omf_timestamp

omf_timestamp: $(omffiles)
	-for omffile in $(omffiles); do \
	  scrollkeeper-preinstall $(helpdir)/$(docname).sgml $$omffile $(omf_dir)/$$omffile; \
	done
	touch omf_timestamp

index.html: $(docname)/index.html
	-cp $(docname)/index.html .

# the wierd srcdir trick is because the db2html from the Cygnus RPMs
# cannot handle relative filenames
$(docname)/index.html: $(srcdir)/$(docname).sgml
	-srcdir=`cd $(srcdir) && pwd`; \
	db2html $$srcdir/$(docname).sgml

app-dist-hook: index.html
	-$(mkinstalldirs) $(distdir)/$(docname)/stylesheet-images
	-$(mkinstalldirs) $(distdir)/figures
	-cp $(srcdir)/$(docname)/*.html $(distdir)/$(docname)
	-cp $(srcdir)/$(docname)/*.css $(distdir)/$(docname)
	-cp $(srcdir)/$(docname)/stylesheet-images/*.gif \
		$(distdir)/$(docname)/stylesheet-images
	-cp $(srcdir)/figures/*.png \
		$(distdir)/figures

install-data-am: index.html omf
	-$(mkinstalldirs) $(DESTDIR)$(helpdir)/stylesheet-images
	-$(mkinstalldirs) $(DESTDIR)$(helpdir)/figures
	-cp $(srcdir)/$(docname).sgml $(DESTDIR)$(helpdir)
	-for file in $(srcdir)/$(docname)/*.html $(srcdir)/$(docname)/*.css; do \
	  basefile=`echo $$file | sed -e 's,^.*/,,'`; \
	  $(INSTALL_DATA) $$file $(DESTDIR)$(helpdir)/$$basefile; \
	done
	-for file in $(srcdir)/figures/*.png; do \
	  basefile=`echo $$file | sed -e  's,^.*/,,'`; \
	  $(INSTALL_DATA) $$file $(DESTDIR)$(helpdir)/figures/$$basefile; \
	done
	-for file in $(srcdir)/$(docname)/stylesheet-images/*.gif; do \
	  basefile=`echo $$file | sed -e  's,^.*/,,'`; \
	  $(INSTALL_DATA) $$file $(DESTDIR)$(helpdir)/stylesheet-images/$$basefile; \
	done

$(docname).ps: $(srcdir)/$(docname).sgml
	-srcdir=`cd $(srcdir) && pwd`; \
	db2ps $$srcdir/$(docname).sgml

$(docname).rtf: $(srcdir)/$(docname).sgml
	-srcdir=`cd $(srcdir) && pwd`; \
	db2ps $$srcdir/$(docname).sgml

