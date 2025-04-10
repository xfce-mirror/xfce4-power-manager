XDT_VERSION_INIT([4.21.0], [git])
m4_define([copyright_year], [2025])

AC_COPYRIGHT([Copyright (c) 2008-copyright_year() The Xfce development team. All rights reserved.])
AC_INIT([xfce4-power-manager],[xdt_version],[https://gitlab.xfce.org/xfce/xfce4-power-manager])
AC_PREREQ([2.69])
AC_REVISION([xdt_version_build])
AC_CONFIG_MACRO_DIRS([m4])
AC_DEFINE([VERSION_FULL], [PACKAGE_VERSION], [Alias for VERSION and PACKAGE_VERSION for meson compatibility])
COPYRIGHT_YEAR=copyright_year()
AC_DEFINE_UNQUOTED([COPYRIGHT_YEAR], ["$COPYRIGHT_YEAR"], [Copyright year])
AC_SUBST([COPYRIGHT_YEAR])

# subdir-objects is currently broken, see
# http://debbugs.gnu.org/cgi/bugreport.cgi?bug=13928
AM_INIT_AUTOMAKE([1.8 dist-bzip2 tar-ustar no-dist-gzip foreign subdir-objects])
AC_CONFIG_HEADERS([config.h])
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

AC_USE_SYSTEM_EXTENSIONS()

# ===================================================== #
#               Basic compiler settings                 #
# ===================================================== #
AC_PROG_CC()
AM_PROG_CC_C_O()
LT_PATH_LD([])
AC_PROG_INSTALL()

# ===================================================== #
#               Initialize libtool                      #
# ===================================================== #
LT_PREREQ([2.2.6])
LT_INIT([disable-static])

# ==================================================== #
#   Check for headers needed for standard interfaces   #
# ==================================================== #
AC_CHECK_HEADERS([errno.h signal.h stddef.h sys/types.h memory.h stdlib.h   \
                  string.h sys/stat.h sys/user.h sys/wait.h time.h math.h   \
                  unistd.h sys/resource.h sys/socket.h sys/sysctl.h fcntl.h \
                  sys/param.h procfs.h ])

AC_CHECK_FUNCS([setsid sigprocmask])

# ===================================================== #
# 		Check for libm	 			#
# ===================================================== #
AC_CHECK_LIB([m], [round])


# ===================================================== #
# 		Check for i18n support 			#
# ===================================================== #
GETTEXT_PACKAGE="$PACKAGE"
AC_DEFINE_UNQUOTED([GETTEXT_PACKAGE], ["$GETTEXT_PACKAGE"], [Name of default gettext domain])
AC_SUBST([GETTEXT_PACKAGE])
AM_GNU_GETTEXT([external])
AM_GNU_GETTEXT_VERSION([0.19.8])

#==================================================== #
#          Check for required packages 		      #
#=====================================================#
m4_define([gtk_minimum_version], [3.24.0])
m4_define([glib_minimum_version], [2.72.0])
m4_define([xfce4_minimum_version], [4.18.0])
m4_define([libxfce4ui_minimum_version],[4.21.0])
m4_define([libxfce4util_minimum_version],[4.19.4])
m4_define([libnotify_minimum_version], [0.7.8])
m4_define([upower_minimum_version], [0.99.10])

m4_define([x11_minimum_version], [1.6.7])
m4_define([xrandr_minimum_version], [1.5.0])
m4_define([xext_minimum_version], [1.0.0])

m4_define([wayland_minimum_version], [1.20])
m4_define([wayland_protocols_minimum_version], [1.25])

m4_define([polkit_minimum_version], [0.102])

XDT_CHECK_PACKAGE([GTK], [gtk+-3.0], [gtk_minimum_version])
XDT_CHECK_PACKAGE([GLIB], [glib-2.0], [glib_minimum_version])
XDT_CHECK_PACKAGE([GOBJECT], [gobject-2.0], [glib_minimum_version])
XDT_CHECK_PACKAGE([GTHREAD], [gthread-2.0], [glib_minimum_version])
XDT_CHECK_PACKAGE([GMODULE], [gmodule-2.0], [glib_minimum_version])
XDT_CHECK_PACKAGE([GIO], [gio-2.0], [glib_minimum_version])
XDT_CHECK_PACKAGE([GIO_UNIX], [gio-unix-2.0], [glib_minimum_version])
XDT_CHECK_PACKAGE([XFCONF], [libxfconf-0],[xfce4_minimum_version])
XDT_CHECK_PACKAGE([LIBXFCE4UI], [libxfce4ui-2],[libxfce4ui_minimum_version])
XDT_CHECK_PACKAGE([LIBXFCE4UTIL], [libxfce4util-1.0],[libxfce4util_minimum_version])
XDT_CHECK_PACKAGE([LIBNOTIFY],[libnotify], [libnotify_minimum_version])
XDT_CHECK_PACKAGE([UPOWER],[upower-glib], [upower_minimum_version])

XDT_CHECK_PACKAGE_BINARY([GDBUS_CODEGEN], [gio-2.0], [gdbus_codegen], [gdbus-codegen])
XDT_CHECK_PACKAGE_BINARY([GLIB_COMPILE_RESOURCES], [gio-2.0], [glib_compile_resources], [glib-compile-resources])

#==================================================== #
#          Check for optional packages 		      #
#=====================================================#
XDT_CHECK_OPTIONAL_FEATURE([X11],
                           [x11],
                           [
                             XDT_FEATURE_DEPENDENCY([GDK_X11], [gdk-x11-3.0], [gtk_minimum_version])
                             XDT_FEATURE_DEPENDENCY([LIBX11], [x11], [x11_minimum_version])
                             XDT_FEATURE_DEPENDENCY([XRANDR], [xrandr], [xrandr_minimum_version])
                             XDT_FEATURE_DEPENDENCY([LIBXEXT], [xext], [xext_minimum_version])
                           ],
                           [the X11 windowing system])
XDT_CHECK_OPTIONAL_FEATURE([WAYLAND],
                           [wayland],
                           [
                             XDT_FEATURE_DEPENDENCY([GDK_WAYLAND], [gdk-wayland-3.0], [gtk_minimum_version])
                             XDT_FEATURE_DEPENDENCY([WAYLAND_CLIENT], [wayland-client], [wayland_minimum_version])
                             XDT_FEATURE_DEPENDENCY([WAYLAND_SCANNER], [wayland-scanner], [wayland_minimum_version])
                             XDT_FEATURE_DEPENDENCY([WAYLAND_PROTOCOLS], [wayland-protocols], [wayland_protocols_minimum_version])
                           ],
                           [the Wayland windowing system])
if test x"$ENABLE_X11" != x"yes" -a x"$ENABLE_WAYLAND" != x"yes"; then
  AC_MSG_ERROR([Either both X11 and Wayland support was disabled, or required dependencies are missing. One of the two must be enabled.])
fi

if test x"$ENABLE_X11" = x"yes"; then
  AC_CHECK_DECL(XF86XK_Suspend, have_XF86XK_Suspend=1, have_XF86XK_Suspend=0, [#include <X11/XF86keysym.h>])
  if test x"${ac_cv_have_decl_XF86XK_Suspend}" = x"yes"; then
    AC_DEFINE(HAVE_XF86XK_SUSPEND, 1, [Define if XF86XK_Suspend is available])
  fi
  AC_CHECK_DECL(XF86XK_Hibernate, have_XF86XK_Hibernate=1, have_XF86XK_Hibernate=0, [#include <X11/XF86keysym.h>])
  if test x"${ac_cv_have_decl_XF86XK_Hibernate}" = x"yes"; then
    AC_DEFINE(HAVE_XF86XK_HIBERNATE, 1, [Define if XF86XK_Hibernate is available])
  fi
fi

if test x"$ENABLE_WAYLAND" = x"yes"; then
  WAYLAND_PROTOCOLS_PKGDATADIR=`$PKG_CONFIG --variable=pkgdatadir wayland-protocols`
  AC_SUBST([WAYLAND_PROTOCOLS_PKGDATADIR])
  WLR_PROTOCOLS_PKGDATADIR=`$PKG_CONFIG --variable=pkgdatadir wlr-protocols 2>/dev/null || true`
  if test x"$WLR_PROTOCOLS_PKGDATADIR" = x""; then
    WLR_PROTOCOLS_PKGDATADIR="$ac_abs_confdir/protocols/wlr-protocols"
    if test "$(echo "$WLR_PROTOCOLS_PKGDATADIR/"*)" = "$WLR_PROTOCOLS_PKGDATADIR/*"; then
      AC_MSG_ERROR([$WLR_PROTOCOLS_PKGDATADIR seems empty, did you clone the wlr-protocols submodule?])
    fi
  fi
  AC_MSG_CHECKING([for wlr-protocols])
  AC_MSG_RESULT([$WLR_PROTOCOLS_PKGDATADIR])
  AC_SUBST([WLR_PROTOCOLS_PKGDATADIR])
fi
dnl FIXME: Bump wayland_protocols_minimum_version to 1.27 when it is an acceptable requirement,
dnl and remove this and protocols/ext-idle-notify-v1.xml
AM_CONDITIONAL([HAVE_IDLE_NOTIFY], [test -f "$WAYLAND_PROTOCOLS_PKGDATADIR/staging/ext-idle-notify/ext-idle-notify-v1.xml"])

#=======================================================#
#              Polkit?			                #
#=======================================================#
XDT_CHECK_OPTIONAL_PACKAGE([POLKIT], [polkit-gobject-1], [polkit_minimum_version], [polkit], [Polkit support])

#=======================================================#
#                 Panel plugin ?                        #
#=======================================================#
XDT_CHECK_OPTIONAL_PACKAGE([LIBXFCE4PANEL], [libxfce4panel-2.0], [xfce4_minimum_version],
[xfce4panel], [To build plugin support for Xfce's panel])


#=======================================================#
#     Compile time default choice of backend            #
#=======================================================#
AC_ARG_WITH([backend],
	    AS_HELP_STRING([--with-backend=<option>],
			   [Default backend to use linux, freebsd, openbsd]))
# default to a sane option
AC_CANONICAL_HOST
if test x$with_backend = x; then
	AS_CASE([$host],
		[*-linux*],   [with_backend=linux],
		[*-*freebsd*], [with_backend=freebsd],
		[*-*dragonfly*], [with_backend=freebsd],
		[*-openbsd*|*-bitrig*], [with_backend=openbsd])
fi
AC_DEFINE_UNQUOTED(BACKEND, "$with_backend", [backend])
AC_SUBST(BACKEND, "$with_backend")

if test x$with_backend = xlinux; then
    AC_DEFINE(BACKEND_TYPE_LINUX, 1, [Linux suspend/hibernate backend])
fi
if test x$with_backend = xfreebsd; then
    AC_DEFINE(BACKEND_TYPE_FREEBSD, 1, [FreeBSD suspend/hibernate backend])
fi
if test x$with_backend = xopenbsd; then
    AC_DEFINE(BACKEND_TYPE_OPENBSD, 1, [OpenBSD suspend/hibernate backend])
fi

dnl define a short version string for xfce4-power-manager
m4_append([xfpm_version_short], ["])
m4_append([xfpm_version_short], [xdt_version_major])
m4_append([xfpm_version_short], [.])
m4_append([xfpm_version_short], [xdt_version_minor])
m4_append([xfpm_version_short], ["])

dnl define xfpm's short version (major.minor)
AC_DEFINE([VERSION_SHORT], [xfpm_version_short], [Short version string of xfce4-power-manager])

#=======================================================#
#              Check for debugging support              #
#=======================================================#
XDT_FEATURE_DEBUG([xdt_debug_default])

#=======================================================#
#              Substitute platform flags                #
#=======================================================#
AC_MSG_CHECKING([PLATFORM_CPPFLAGS])
AC_MSG_RESULT([$PLATFORM_CPPFLAGS])
AC_SUBST([PLATFORM_CPPFLAGS])
AC_MSG_CHECKING([PLATFORM_CFLAGS])
AC_MSG_RESULT([$PLATFORM_CFLAGS])
AC_SUBST([PLATFORM_CFLAGS])
AC_MSG_CHECKING([PLATFORM_LDFLAGS])
AC_MSG_RESULT([$PLATFORM_LDFLAGS])
AC_SUBST([PLATFORM_LDFLAGS])

AC_CONFIG_FILES([
Makefile
protocols/Makefile
libdbus/Makefile
common/Makefile
src/Makefile
settings/Makefile
panel-plugin/Makefile
data/Makefile
data/icons/Makefile
data/icons/16x16/Makefile
data/icons/24x24/Makefile
data/icons/32x32/Makefile
data/icons/48x48/Makefile
data/icons/64x64/Makefile
data/icons/96x96/Makefile
data/icons/128x128/Makefile
data/icons/scalable/Makefile
data/interfaces/Makefile
data/appdata/Makefile
po/Makefile.in
])
AC_OUTPUT

echo "prefix:               ${prefix}"
echo "xdg autostart:        ${sysconfdir}/xdg/autostart"
echo "X11 Support:          ${ENABLE_X11:-no}"
echo "Wayland Support:      ${ENABLE_WAYLAND:-no}"
echo "POLKIT:               ${POLKIT_FOUND:-no}"
echo "Panel plugin:         ${LIBXFCE4PANEL_FOUND:-no}"
echo "Backend:              ${with_backend}"
echo "Debug:                ${enable_debug}"
echo
