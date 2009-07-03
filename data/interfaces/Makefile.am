interfaces_h =					\
	xfpm-settings_ui.h

interfaces =					\
	xfpm-settings.ui

if MAINTAINER_MODE
BUILT_SOURCES = $(interfaces_h)
xfpm-settings_ui.h: xfpm-settings.ui
	exo-csource --static --strip-comments --strip-content --name=xfpm_settings_ui $< > $@
endif

DISTCLEANFILES =				\
	$(interfaces_h)

EXTRA_DIST =					\
	$(interfaces)				\
	$(interfaces_h)
