NULL=

LANG=C

DOC_MODULE=nautilus
OMFFILE=nautilus-C.omf
SGMLFILE=nautilus.sgml
SGML_DIR=$(datadir)/gnome/help
OMF_DIR=$(top_srcdir)/omf-install

TARGET_DIR=$(SGML_DIR)/$(DOC_MODULE)/$(LANG)/

EXTRA_DIST = $(OMFFILE)

all: omf

omf: $(OMFFILE)
	-scrollkeeper-preinstall $(DESTDIR)$(TARGET_DIR)$(SGMLFILE) $(OMFFILE) $(OMF_DIR)/$(OMFFILE)


install-data-local: 
	$(mkinstalldirs) $(DESTDIR)$(TARGET_DIR) 
	installfiles=`echo $(srcdir)/sgml/*.sgml 		\
	$(srcdir)/img/*.png					\
	$(srcdir)/html/*.html`;					\
	if test "$$installfiles" = '$(srcdir)/sgml/*.sgml'; 	\
	then echo '-- Nothing to install' ; 			\
	else 							\
	for i in $$installfiles; do				\
		basefile=`basename $$i`;  			\
		$(INSTALL_DATA) $$i $(DESTDIR)$(TARGET_DIR)$$basefile; 	\
	done; 							\
	fi

dist-hook:
	mkdir $(distdir)/sgml 
	mkdir $(distdir)/html 
	mkdir $(distdir)/img
	-cp $(srcdir)/sgml/*.sgml $(distdir)/sgml
	-cp $(srcdir)/img/*.png $(distdir)/img
	-cp $(srcdir)/html/*.html $(distdir)/html

