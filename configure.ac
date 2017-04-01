AC_PREREQ([2.69])


dnl ***********************************************************************
dnl Define Versioning Information
dnl ***********************************************************************
m4_define([major_version],[1])
m4_define([minor_version],[0])
m4_define([micro_version],[2])
m4_define([package_version],[major_version.minor_version.micro_version])
m4_define([bug_report_url],[https://bugzilla.gnome.org/enter_bug.cgi?product=recipes])
m4_define([api_version],[major_version])

AX_IS_RELEASE([micro-version])

dnl ***********************************************************************
dnl Initialize autoconf
dnl ***********************************************************************
AC_INIT([gnome-recipes],[package_version],[bug_report_url])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_SRCDIR([configure.ac])
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_MACRO_DIRS([m4])
AC_SUBST([ACLOCAL_AMFLAGS], "-I m4 -I libgd")
AC_CANONICAL_HOST

dnl ***********************************************************************
dnl Add extra debugging with --enable-debug and --enable-compile-warnings
dnl ***********************************************************************
AX_CHECK_ENABLE_DEBUG([no],[]
                      [G_DISABLE_ASSERT G_DISABLE_CHECKS G_DISABLE_CAST_CHECKS])

AC_USE_SYSTEM_EXTENSIONS


dnl ***********************************************************************
dnl Make version information available to autoconf files
dnl ***********************************************************************
AC_SUBST([MAJOR_VERSION],major_version)
AC_SUBST([MINOR_VERSION],minor_version)
AC_SUBST([MICRO_VERSION],micro_version)
AC_SUBST([API_VERSION],api_version)


dnl ***********************************************************************
dnl Initialize automake
dnl ***********************************************************************
AM_SILENT_RULES([yes])
AM_INIT_AUTOMAKE([1.11 foreign subdir-objects tar-ustar no-dist-gzip dist-xz -Wno-portability])
AM_MAINTAINER_MODE([enable])
AX_GENERATE_CHANGELOG


dnl ***********************************************************************
dnl Internationalization
dnl ***********************************************************************
GETTEXT_PACKAGE=AC_PACKAGE_TARNAME
AC_SUBST([GETTEXT_PACKAGE])
AC_DEFINE_UNQUOTED([GETTEXT_PACKAGE], ["$GETTEXT_PACKAGE"], [GETTEXT package name])

AM_GNU_GETTEXT_VERSION([0.19.7])
AM_GNU_GETTEXT([external])


dnl ***********************************************************************
dnl Check for required programs
dnl ***********************************************************************
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_SED
AC_PATH_PROG([GLIB_GENMARSHAL],[glib-genmarshal])
AC_PATH_PROG([GLIB_MKENUMS],[glib-mkenums])
AC_PATH_PROG([GLIB_COMPILE_RESOURCES],[glib-compile-resources])
AC_PATH_PROG([APPSTREAM_UTIL],[appstream-util])
PKG_PROG_PKG_CONFIG([0.22])
GLIB_GSETTINGS
GOBJECT_INTROSPECTION_CHECK([1.42.0])

VAPIGEN_CHECK



AX_COMPILER_FLAGS


dnl ***********************************************************************
dnl Ensure C11 is Supported
dnl ***********************************************************************
AX_CHECK_COMPILE_FLAG([-std=gnu11],
                      [CFLAGS="$CFLAGS -std=gnu11"],
                      [AC_MSG_ERROR([C compiler cannot compile GNU C11 code])])



dnl ***********************************************************************
dnl Build options
dnl ***********************************************************************
AC_ARG_ENABLE(gspell,
              [AS_HELP_STRING([--enable-gspell],
                              [Use gspell for spell-checking])],,
              [enable_gspell="yes"])

if test "x$enable_gspell" = xyes; then
        GSPELL_DEP=gspell-1
        AC_DEFINE([ENABLE_GSPELL], [1], [Define to 1 if gspell is used])
        GSPELL_VERSION="$($PKG_CONFIG --modversion gspell-1)"
        AC_SUBST([GSPELL_VERSION])
        AC_DEFINE_UNQUOTED([GSPELL_VERSION], ["${GSPELL_VERSION}"], [The version of gspell])
fi

AC_ARG_ENABLE(autoar,
              [AS_HELP_STRING([--enable-autoar],
                              [Use gnome-autoar for export/import support])],,
              [enable_autoar="yes"])
if test "x$enable_autoar" = xyes; then
        AUTOAR_DEP=gnome-autoar-0
        AC_DEFINE([ENABLE_AUTOAR], [1], [Define to 1 if gnome-autoar is used])
        AUTOAR_VERSION="$($PKG_CONFIG --modversion $AUTOAR_DEP)"
        AC_SUBST([AUTOAR_VERSION])
        AC_DEFINE_UNQUOTED([AUTOAR_VERSION], ["${AUTOAR_VERSION}"], [The version of gnome-autoar])
fi
AM_CONDITIONAL(USE_AUTOAR, test "x$enable_autoar" = xyes)

AC_ARG_ENABLE(canberra,
              [AS_HELP_STRING([--enable-canberra],
                              [Use libcanberra for sound support])],,
              [enable_canberra="yes"])
if test "x$enable_canberra" = xyes; then
        CANBERRA_DEP=libcanberra
        AC_DEFINE([ENABLE_CANBERRA], [1], [Define to 1 if libcanberra is used])
        CANBERRA_VERSION="$($PKG_CONFIG --modversion $CANBERRA_DEP)"
        AC_SUBST([CANBERRA_VERSION])
        AC_DEFINE_UNQUOTED([CANBERRA_VERSION], ["${CANBERRA_VERSION}"], [The version of libcanberra])
fi

dnl ***********************************************************************
dnl Check for required packages
dnl ***********************************************************************
PKG_CHECK_MODULES(RECIPES, [gmodule-2.0
                            gio-2.0 >= 2.42
                            gtk+-3.0 >= 3.22
                            $AUTOAR_DEP
                            $GSPELL_DEP
                            $CANBERRA_DEP])

LIBGD_INIT([tagged-entry static])

dnl ***********************************************************************
dnl Initialize Libtool
dnl ***********************************************************************
LT_PREREQ([2.2])
LT_INIT

dnl ***********************************************************************
dnl Help
dnl ***********************************************************************

YELP_HELP_INIT

dnl ***********************************************************************
dnl Process .in Files
dnl ***********************************************************************
AC_CONFIG_FILES([
	Makefile
        libgd/Makefile
	src/Makefile
	tools/Makefile
	data/Makefile
	tests/Makefile
	help/Makefile
	po/Makefile.in
	po-data/Makefile.in
],[],
[API_VERSION='$API_VERSION'])
AC_OUTPUT

echo ""
echo " ${PACKAGE} - ${VERSION}"
echo ""
echo " Options"
echo ""
echo "  Prefix ............................... : ${prefix}"
echo "  Libdir ............................... : ${libdir}"
echo "  Export/Import ........................ : ${enable_autoar}"
echo "  Spell checking ....................... : ${enable_gspell}"
echo "  Sound ................................ : ${enable_canberra}"
echo ""
