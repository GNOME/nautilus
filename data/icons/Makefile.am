icon16dir = $(datadir)/icons/hicolor/16x16/apps
icon16_DATA = data/icons/hicolor/16x16/apps/org.gnome.Nautilus.png
icon22dir = $(datadir)/icons/hicolor/22x22/apps
icon22_DATA = data/icons/hicolor/22x22/apps/org.gnome.Nautilus.png
icon24dir = $(datadir)/icons/hicolor/24x24/apps
icon24_DATA = data/icons/hicolor/24x24/apps/org.gnome.Nautilus.png
icon32dir = $(datadir)/icons/hicolor/32x32/apps
icon32_DATA = data/icons/hicolor/32x32/apps/org.gnome.Nautilus.png
icon48dir = $(datadir)/icons/hicolor/48x48/apps
icon48_DATA = data/icons/hicolor/48x48/apps/org.gnome.Nautilus.png
icon256dir = $(datadir)/icons/hicolor/256x256/apps
icon256_DATA = data/icons/hicolor/256x256/apps/org.gnome.Nautilus.png
iconsymbolicdir = $(datadir)/icons/hicolor/symbolic/apps
iconsymbolic_DATA = data/icons/hicolor/symbolic/apps/org.gnome.Nautilus-symbolic.svg

icon_files = \
	$(icon16_DATA) \
	$(icon22_DATA) \
	$(icon24_DATA) \
	$(icon32_DATA) \
	$(icon48_DATA) \
	$(icon256_DATA) \
	$(iconsymbolic_DATA) \
	data/icons/src/org.gnome.Nautilus.svg

gtk_update_icon_cache = gtk-update-icon-cache -f -t $(datadir)/icons/hicolor

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

EXTRA_DIST = $(icon_files)
