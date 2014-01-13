INCLUDES = \
	-I$(top_srcdir) \
	-DG_LOG_DOMAIN=\"xfce4-brightness-plugin\" \
	-DPACKAGE_LOCALE_DIR=\"$(localedir)\" \
	$(PLATFORM_CPPFLAGS)

#
# xfce4 brightness plugin
#
plugin_LTLIBRARIES = \
	libxfce4brightness.la

plugindir = \
	$(libdir)/xfce4/panel/plugins

libxfce4brightness_la_SOURCES =		\
	brightness-plugin.c			\
	brightness-button.c			\
	brightness-button.h

libxfce4brightness_la_CFLAGS =		\
	-I$(top_srcdir)				\
	-I$(top_srcdir)/src			\
	-DLOCALEDIR=\"$(localedir)\"  		\
	$(LIBXFCE4PANEL_CFLAGS)       		\
	$(LIBXFCE4UTIL_CFLAGS)        		\
	$(LIBXFCE4UI_CFLAGS)			\
	$(GTK_CFLAGS)			      	\
	$(GLIB_CFLAGS)				\
	$(XRANDR_CFLAGS)			\
	$(PLATFORM_CPPFLAGS)			\
	$(PLATFORM_CFLAGS)

libxfce4brightness_la_LDFLAGS =		\
       -avoid-version \
       -module \
       -no-undefined \
       -export-symbols-regex '^xfce_panel_module_(preinit|init|construct)' \
       $(PLATFORM_LDFLAGS)

libxfce4brightness_la_LIBADD =			\
	$(top_builddir)/common/libxfpmcommon.la \
	$(LIBXFCE4PANEL_LIBS)   		\
	$(LIBXFCE4UI_LIBS)			\
	$(LIBXFCE4UTIL_LIBS)			\
	$(GTK_LIBS)   				\
	$(GLIB_LIBS)				\
	$(XRANDR_LIBS)

#
# Desktop file
#
desktopdir = $(datadir)/xfce4/panel-plugins
desktop_in_in_files = xfce4-brightness-plugin.desktop.in.in
desktop_in_files = $(desktop_in_in_files:.desktop.in.in=.desktop.in)
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)
@INTLTOOL_DESKTOP_RULE@

EXTRA_DIST =                                                            \
        $(desktop_in_in_files)

CLEANFILES =                                                            \
        $(desktop_in_files)                                             \
        $(desktop_DATA)

%.desktop.in: %.desktop.in.in
	sed -e "s,\@libdir\@,$(libdir),g" < $< > $@
