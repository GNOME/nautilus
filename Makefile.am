NULL=

SUBDIRS =\
	librsvg \
	libnautilus \
	libnautilus-extensions \
	nautilus-widgets \
	src \
	components \
	icons \
	data \
	intl \
	po \
	$(NULL)

EXTRA_DIST=\
	COPYING.LIB \
	nautilus.spec.in \
	$(NULL)

dist-hook: nautilus.spec
	cp nautilus.spec $(distdir)
