NULL =
themedir = $(datadir)/icons/hicolor
size = 24x24
context = apps

iconsdir = $(themedir)/$(size)/$(context)

icons_DATA =		\
	nautilus.png	\
	$(NULL)

EXTRA_DIST =		\
	$(icons_DATA)

gtk_update_icon_cache = gtk-update-icon-cache -f -t $(datadir)/icons/hicolor

install-data-hook:
	@-if test -z "$(DESTDIR)"; then \
		echo "Updating Gtk icon cache."; \
		$(gtk_update_icon_cache); \
	else \
		echo "*** Icon cache not updated.  After install, run this:"; \
		echo "***   $(gtk_update_icon_cache)"; \
	fi
