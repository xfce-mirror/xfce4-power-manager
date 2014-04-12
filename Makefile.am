@SET_MAKE@

if BUILD_PANEL_PLUGINS
plugins_dir = panel-plugins
endif

SUBDIRS =        	\
	data		\
	libdbus		\
	common		\
	src		\
	settings	\
	$(plugins_dir)	\
	po


EXTRA_DIST = 			\
	TODO			\
	intltool-extract.in 	\
	intltool-merge.in 	\
	intltool-update.in

DISTCLEANFILES = \
	intltool-extract \
	intltool-merge \
	intltool-update

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

dist-bz2: dist
	zcat $(PACKAGE)-$(VERSION).tar.gz | \
	bzip2 --best -c > $(PACKAGE)-$(VERSION).tar.bz2

distcheck-bz2: distcheck
	zcat $(PACKAGE)-$(VERSION).tar.gz | \
	bzip2 --best -c > $(PACKAGE)-$(VERSION).tar.bz2
         
snapshot: dist
	mv $(PACKAGE)-$(VERSION).tar.gz \
	$(PACKAGE)-$(VERSION)-r@REVISION@.tar.gz

snapshot-bz2: dist-bz2
	mv $(PACKAGE)-$(VERSION).tar.bz2 \
	$(PACKAGE)-$(VERSION)-r@REVISION@.tar.bz2
