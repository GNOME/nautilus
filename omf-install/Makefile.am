omf_dest_dir=$(datadir)/omf/nautilus

install-data-local:
	$(mkinstalldirs) $(DESTDIR)$(omf_dest_dir)
	-for file in $(srcdir)/*.omf; do 			\
		$(INSTALL_DATA) $(srcdir)/$$file $(DESTDIR)$(omf_dest_dir); \
	done
	-scrollkeeper-update

uninstall-local:
	-for file in $(srcdir)/*.omf; do 			\
		basefile=`basename $$file`; 			\
		rm -f $(omf_dest_dir)/$$basefile; 		\
        done
	-scrollkeeper-update
