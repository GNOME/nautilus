# To use this template:
#     1) Define: figs, docname, lang, omffile, sgml_ents although figs, 
#        omffile, and sgml_ents may be empty in your Makefile.am which 
#        will "include" this one 
#     2) Figures must go under figures/ and be in PNG format
#     3) You should only have one document per directory 
#
#        Note that this makefile forces the directory name under
#        $prefix/share/gnome/help/ to be the same as the SGML filename
#        of the document.  This is required by GNOME. eg:
#        $prefix/share/gnome/help/fish_applet/C/fish_applet.sgml
#                                 ^^^^^^^^^^^   ^^^^^^^^^^^
# Definitions:
#   figs         A list of screenshots which will be included in EXTRA_DIST
#                Note that these should reside in figures/ and should be .png
#                files, or you will have to make modifications below.
#   docname      This is the name of the SGML file: <docname>.sgml
#   lang         This is the document locale
#   omffile      This is the name of the OMF file.  Convention is to name
#                it <docname>-<locale>.omf.
#   sgml_ents    This is a list of SGML entities which must be installed 
#                with the main SGML file and included in EXTRA_DIST. 
# eg:
#   figs = \
#          figures/fig1.png            \
#          figures/fig2.png
#   docname = scrollkeeper-manual
#   lang = C
#   omffile=scrollkeeper-manual-C.omf
#   sgml_ents = fdl.sgml
#   include $(top_srcdir)/help/sgmldocs.make
#   dist-hook: app-dist-hook
#

docdir = $(datadir)/gnome/help/$(docname)/$(lang)

doc_DATA = index.html

sgml_files = $(sgml_ents) $(docname).sgml

omf_dir=$(top_srcdir)/omf-install

EXTRA_DIST = $(sgml_files) $(doc_DATA) $(omffile) $(figs)

CLEANFILES = omf_timestamp

all: index.html omf

omf: omf_timestamp

omf_timestamp: $(omffile)
	-for file in $(omffile); do \
	  scrollkeeper-preinstall $(docdir)/$(docname).sgml $$file $(omf_dir)/$$file; \
	done
	touch omf_timestamp

index.html: $(docname)/index.html
	-cp $(docname)/index.html .

$(docname).sgml: $(sgml_ents)
        -ourdir=`pwd`;  \
        cd $(srcdir);   \
        cp $(sgml_ents) $$ourdir


# The weird srcdir trick is because the db2html from the Cygnus RPMs
# cannot handle relative filenames
$(docname)/index.html: $(srcdir)/$(docname).sgml
	-srcdir=`cd $(srcdir) && pwd`;			\
	if test "$(HAVE_JW)" = 'yes' ; then 		\
		jw -c /etc/sgml/catalog $$srcdir/$(docname).sgml -o $$srcdir/$(docname); \
	else 						\
		db2html $$srcdir/$(docname).sgml; 	\
	 fi

app-dist-hook: index.html
	-$(mkinstalldirs) $(distdir)/$(docname)/stylesheet-images
	-$(mkinstalldirs) $(distdir)/figures
	-cp $(srcdir)/$(docname)/*.html $(distdir)/$(docname)
	-for file in $(srcdir)/$(docname)/*.css; do \
	  basefile=`echo $$file | sed -e  's,^.*/,,'`; \
	  cp $$file $(distdir)/$(docname)/$$basefile ; \
	done
	-for file in $(srcdir)/$(docname)/stylesheet-images/*.gif; do \
	  basefile=`echo $$file | sed -e  's,^.*/,,'`; \
	  cp $$file $(distdir)/$(docname)/stylesheet-images/$$basefile ; \
	done
	-if [ -e topic.dat ]; then \
		cp $(srcdir)/topic.dat $(distdir); \
	 fi

install-data-am: index.html omf
	-$(mkinstalldirs) $(DESTDIR)$(docdir)/stylesheet-images
	-$(mkinstalldirs) $(DESTDIR)$(docdir)/figures
	-cp $(srcdir)/$(sgml_files) $(DESTDIR)$(docdir)
	-for file in $(srcdir)/$(docname)/*.html $(srcdir)/$(docname)/*.css; do \
	  basefile=`echo $$file | sed -e 's,^.*/,,'`; \
	  $(INSTALL_DATA) $$file $(DESTDIR)$(docdir)/$$basefile; \
	done
	-for file in $(srcdir)/figures/*.png; do \
	  basefile=`echo $$file | sed -e  's,^.*/,,'`; \
	  $(INSTALL_DATA) $$file $(DESTDIR)$(docdir)/figures/$$basefile; \
	done
	-for file in $(srcdir)/$(docname)/stylesheet-images/*.gif; do \
	  basefile=`echo $$file | sed -e  's,^.*/,,'`; \
	  $(INSTALL_DATA) $$file $(DESTDIR)$(docdir)/stylesheet-images/$$basefile; \
	done
	-if [ -e $(srcdir)/topic.dat ]; then \
		$(INSTALL_DATA) $(srcdir)/topic.dat $(DESTDIR)$(docdir); \
	 fi

$(docname).ps: $(srcdir)/$(docname).sgml
	-srcdir=`cd $(srcdir) && pwd`; \
	db2ps $$srcdir/$(docname).sgml

$(docname).rtf: $(srcdir)/$(docname).sgml
	-srcdir=`cd $(srcdir) && pwd`; \
	db2ps $$srcdir/$(docname).sgml

uninstall-local:
	-for file in $(srcdir)/$(docname)/stylesheet-images/*.gif; do \
	  basefile=`echo $$file | sed -e  's,^.*/,,'`; \
	  rm -f $(docdir)/stylesheet-images/$$basefile; \
	done
	-for file in $(srcdir)/figures/*.png; do \
	  basefile=`echo $$file | sed -e  's,^.*/,,'`; \
	  rm -f $(docdir)/figures/$$basefile; \
	done
	-for file in $(srcdir)/$(docname)/*.html $(srcdir)/$(docname)/*.css; do \
	  basefile=`echo $$file | sed -e 's,^.*/,,'`; \
	  rm -f $(DESTDIR)$(docdir)/$$basefile; \
	done
	-for file in $(sgml_files); do \
	  rm -f $(DESTDIR)$(docdir)/$$file; \
	done
	-rmdir $(DESTDIR)$(docdir)/stylesheet-images
	-rmdir $(DESTDIR)$(docdir)/figures
	-rmdir $(DESTDIR)$(docdir)
