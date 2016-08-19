icondir = $(datadir)/icons
nobase_icon_DATA = \
	$(srcdir)/hicolor/16x16/apps/org.gnome.Nautilus.png \
	$(srcdir)/hicolor/22x22/apps/org.gnome.Nautilus.png \
	$(srcdir)/hicolor/24x24/apps/org.gnome.Nautilus.png \
	$(srcdir)/hicolor/32x32/apps/org.gnome.Nautilus.png \
	$(srcdir)/hicolor/48x48/apps/org.gnome.Nautilus.png \
	$(srcdir)/hicolor/256x256/apps/org.gnome.Nautilus.png \
	$(srcdir)/hicolor/symbolic/apps/org.gnome.Nautilus-symbolic.svg

gtk_update_icon_cache = gtk-update-icon-cache -f -t $(icondir)/hicolor

install-data-hook: update-icon-cache
uninstall-hook: update-icon-cache
update-icon-cache:
	@-if test -z "$(DESTDIR)"; then \
		echo "Updating Gtk icon cache."; \
		$(gtk_update_icon_cache); \
	else \
		echo "*** Icon cache not updated.  After (un)install, run this:"; \
		echo "***   $(gtk_update_icon_cache)"; \
	fi

EXTRA_DIST = \
	$(nobase_icon_DATA) \
	$(srcdir)/src/org.gnome.Nautilus.svg

-include $(top_srcdir)/git.mk
