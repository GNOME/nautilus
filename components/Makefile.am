NULL=

if EAZEL_SERVICES
SERVICE_SUBDIRS = services
endif

if BUILD_MOZILLA_COMPONENT
MOZILLA_COMPONENT_SUBDIRS = mozilla
endif

if BUILD_RPM_COMPONENTS
RPMVIEW_COMPONENT_SUBDIRS = rpmview
endif

SUBDIRS = 					\
	adapter 				\
	hardware 				\
	help 					\
	history 				\
	image-viewer 				\
	loser 					\
	music 					\
	notes 					\
	sample 					\
	text 					\
	throbber 				\
	tree 					\
	$(SERVICE_SUBDIRS) 			\
	$(MOZILLA_COMPONENT_SUBDIRS) 		\
	$(RPMVIEW_COMPONENT_SUBDIRS)		\
	$(NULL)


