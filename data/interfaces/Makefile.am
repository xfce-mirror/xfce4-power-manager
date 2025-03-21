xfpm_resources_deps = $(shell $(GLIB_COMPILE_RESOURCES) --generate-dependencies --sourcedir $(srcdir) $(srcdir)/xfpm.gresource.xml)

xfpm-resources.c: $(srcdir)/xfpm.gresource.xml $(xfpm_resources_deps) Makefile
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(srcdir) --generate-source $<

BUILT_SOURCES = \
	xfpm-resources.c \
	$(NULL)

CLEANFILES =					\
	$(BUILT_SOURCES)

EXTRA_DIST =					\
	xfpm-settings.ui \
	xfpm.gresource.xml \
	meson.build \
	$(NULL)
