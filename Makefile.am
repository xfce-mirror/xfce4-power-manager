@SET_MAKE@

ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}

if BUILD_PANEL_PLUGINS
plugins_dir = panel-plugins
endif

SUBDIRS =        	\
	data		\
	libdbus		\
	common		\
	src		\
	settings	\
	$(plugins_dir) \
	po


distuninstallcheck_listfiles = 						\
	find . -type f -print | grep -v ./share/icons/hicolor/icon-theme.cache

.PHONY: ChangeLog

ChangeLog: Makefile
	(GIT_DIR=$(top_srcdir)/.git git log > .changelog.tmp \
	&& mv .changelog.tmp ChangeLog; rm -f .changelog.tmp) \
	|| (touch ChangeLog; echo 'Git directory not found: installing possibly empty changelog.' >&2)

dist-hook: ChangeLog

distclean-local:
	rm -rf *.cache
