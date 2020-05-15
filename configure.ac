AC_PREREQ(2.54)

AC_INIT(src/main.c)
AC_CANONICAL_HOST
AC_CANONICAL_TARGET

AC_PROG_MAKE_SET

AM_SANITY_CHECK
AM_INIT_AUTOMAKE(morla, 0.16.1)
AM_CONFIG_HEADER(config.h)
AM_MAINTAINER_MODE

AC_PROG_CC
AC_PROG_LN_S
AC_ISC_POSIX
AC_PROG_LIBTOOL
AC_PROG_INSTALL
AM_PROG_CC_STDC

AC_C_CONST
AC_C_INLINE

AC_HEADER_STDC
AC_HEADER_DIRENT
AC_CHECK_HEADERS(stdio.h sys/stat.h sys/types.h unistd.h fcntl.h string.h dirent.h malloc.h pwd.h grp.h)

AC_FUNC_VPRINTF

GETTEXT_PACKAGE=morla
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE,"$GETTEXT_PACKAGE", [Gettext])

ALL_LINGUAS="it"
AM_GLIB_GNU_GETTEXT

AM_GLIB_DEFINE_LOCALEDIR(LOCALEDIR)

AC_ARG_WITH(gconf, AC_HELP_STRING([--with-gconf=none|auto (default=auto)],[do you want gconf?]), GCONF=$withval, GCONF=auto)

PKG_CHECK_MODULES(gconf, [
  gconf-2.0 >= 2.10.0
], gconf="gconf-2.0 >= 2.10.0", gconf="")

if test "$GCONF" = none; then
  gconf=""
fi

AC_ARG_WITH(gtkhtml, AC_HELP_STRING([--with-gtkhtml=none|auto (default=auto)],[do you want gtkhtml?]), GTKHTML=$withval, GTKHTML=auto)

PKG_CHECK_MODULES(gtkhtml, [
  libgtkhtml-2.0 >= 2.0.0
], gtkhtml="libgtkhtml-2.0 >= 2.0.0", gtkhtml="")

if test "$GTKHTML" = none; then
  gtkhtml=""
fi

PKG_CHECK_MODULES(gtk, [
  glib-2.0 >= 2.6.0 
  gthread-2.0 >= 2.6.0
  gmodule-2.0 >= 2.6.0
  gtk+-2.0 >= 2.6.0 
  libxslt > 1.0.0
  nxml >= 0.18.0
  libcurl >= 7.15.0
  $gconf
  $gtkhtml
])

LDFLAGS="$LDFLAGS $gtk_LIBS"
CFLAGS="$CFLAGS $gtk_CFLAGS"

AH_TEMPLATE([USE_GCONF], [Define to 1 if we can use libgconf])
AH_TEMPLATE([USE_GTKHTML], [Define to 1 if we can use gtkhtml])

if test "$gconf" != ""; then
  AC_DEFINE(USE_GCONF)
fi

if test "$gtkhtml" != ""; then
  AC_DEFINE(USE_GTKHTML)
fi

PKG_CHECK_MODULES(rasqal, [rasqal], HAVE_RASQAL=yes, HAVE_RASQAL=no)

if test "$HAVE_RASQAL" != yes; then
  AC_CHECK_HEADER(rasqal.h,,AC_MSG_ERROR(librasqal is required to compile morla))
  AC_CHECK_LIB(rasqal,rasqal_new_query,, AC_MSG_ERROR(librasqal is required to compile morla))
else
  LDFLAGS="$LDFLAGS $rasqal_LIBS"
  CFLAGS="$CFLAGS $rasqal_CFLAGS"
fi

AH_TEMPLATE([USE_RASQAL_OLD], [Define to 1 if we can use old rasqal releases])
AC_CHECK_LIB(rasqal,rasqal_new_world,RASQAL_OLD=no, RASQAL_OLD=yes)

if test "$RASQAL_OLD" = yes; then
  AC_DEFINE(USE_RASQAL_OLD)
fi

PKG_CHECK_MODULES(redland, [redland], HAVE_RDF=yes, HAVE_RDF=no)

if test "$HAVE_RDF" != yes; then
  AC_CHECK_HEADER(redland.h,,AC_MSG_ERROR(librdf is required to compile morla))
  AC_CHECK_LIB(rdf,librdf_node_get_type,, AC_MSG_ERROR(librdf is required to compile morla))
else
  LDFLAGS="$LDFLAGS $redland_LIBS"
  CFLAGS="$CFLAGS $redland_CFLAGS"
fi

# JAVASCRIPT CHECK:
AC_ARG_WITH(javascript, AC_HELP_STRING([--with-javascript=none|ngs|mozilla|firefox|xulrunner|auto (default=auto)],[set a javascript engine]), 
  JAVASCRIPT=$withval

  if ! test "$JAVASCRIPT" = none &&
     ! test "$JAVASCRIPT" = ngs &&
     ! test "$JAVASCRIPT" = mozilla &&
     ! test "$JAVASCRIPT" = firefox &&
     ! test "$JAVASCRIPT" = xulrunner &&
     ! test "$JAVASCRIPT" = auto; then
    JAVASCRIPT=auto

  elif test "$JAVASCRIPT" = firefox ||
       test "$JAVASCRIPT" = xulrunner; then
    JAVASCRIPT=mozilla
  fi
  ,
  JAVASCRIPT=auto
)

AH_TEMPLATE([USE_JS], [Define to 1 if we can use a javascript engine])
AH_TEMPLATE([USE_NGS_JS], [Define to 1 if we can use libjs])
AH_TEMPLATE([USE_MOZILLA_JS], [Define to 1 if we can use mozillajs])

if ! test "$JAVASCRIPT" = none; then

  if test "$JAVASCRIPT" = ngs ||
     test "$JAVASCRIPT" = auto; then

    AC_CHECK_HEADER(js.h,HAVE_JS=yes, HAVE_JS=no)

    if test "$HAVE_JS" = yes; then
      AC_CHECK_LIB(js,js_create_interp, HAVE_JS=yes, HAVE_YES=no)
  
      if test "$HAVE_JS" = yes; then
        AC_DEFINE(USE_JS)
        AC_DEFINE(USE_NGS_JS)
        JAVASCRIPT=ngs
        LDFLAGS="$LDFLAGS -ljs"
      fi
    fi
  fi
  
  if test "$JAVASCRIPT" = mozilla ||
     test "$JAVASCRIPT" = auto; then

    if ! test "$HAVE_JS" = yes; then
      PKG_CHECK_MODULES(firefox_js, [firefox-js], HAVE_JS=yes, HAVE_JS=no);

      if test "$HAVE_JS" = yes; then
        LDFLAGS="$LDFLAGS $firefox_js_LIBS"
        CFLAGS="$CFLAGS $firefox_js_CFLAGS"

        AC_DEFINE(USE_JS)
        AC_DEFINE(USE_MOZILLA_JS)
        JAVASCRIPT=firefox
      fi
    fi

    if ! test "$HAVE_JS" = yes; then
      PKG_CHECK_MODULES(xulrunner_js, [xulrunner-js], HAVE_JS=yes, HAVE_JS=no);

      if test "$HAVE_JS" = yes; then
        LDFLAGS="$LDFLAGS $xulrunner_js_LIBS"
        CFLAGS="$CFLAGS $xulrunner_js_CFLAGS"

        AC_DEFINE(USE_JS)
        AC_DEFINE(USE_MOZILLA_JS)
        JAVASCRIPT=xulrunner
      fi
    fi

    if ! test "$HAVE_JS" = yes; then
      AC_CHECK_HEADER(jsapi.h,HAVE_JS=yes, HAVE_JS=no)
  
      if test "$HAVE_JS" = yes; then
        AC_CHECK_LIB(js, JS_NewContext, HAVE_JS=yes, HAVE_YES=no)
   
        if test "$HAVE_JS" = yes; then
          AC_DEFINE(USE_JS)
          AC_DEFINE(USE_MOZILLA_JS)
          LDFLAGS="$LDFLAGS -ljs"
          JAVASCRIPT=mozilla
        fi
      fi
    fi
  fi

  if test "$JAVASCRIPT" != ngs &&
     test "$JAVASCRIPT" != firefox &&
     test "$JAVASCRIPT" != xulrunner &&
     test "$JAVASCRIPT" != mozilla; then
    JAVASCRIPT=none
  fi
fi

# DOT CHECK:
AC_PATH_PROG(dot, dot)
if test "$dot" = ""; then
  AC_MSG_ERROR(No dot (graphviz) found!)
fi

# MACOSX:
AH_TEMPLATE([ENABLE_MACOSX], [Do you want a native macosx building?])

case "$host_os" in
  darwin*) DARWIN=yes; ;;
esac

if test "$DARWIN" = yes; then
  AC_DEFINE(ENABLE_MACOSX)

  PKG_CHECK_MODULES(ige_mac_integration, [
    ige-mac-integration >= 0.6
  ])

  LDFLAGS="$LDFLAGS $ige_mac_integration_LIBS"
  CFLAGS="$CFLAGS $ige_mac_integration_CFLAGS"
  IGE_MAC_INTERATION="ige-mac-integration     : yes"
fi

# Generic stuff:
CFLAGS="$CFLAGS -Wall -DDOT_CMD=\\\"$dot\\\""

AC_OUTPUT([
Makefile
po/Makefile.in
src/Makefile
icons/Makefile
docs/Makefile
modules/Makefile
morla.1
])

AC_MSG_RESULT([
Morla build summary:
  host                    : $host_os

  glib                    : yes
  gtk+                    : yes
  libxslt                 : yes
  nxml                    : yes
  libcurl                 : yes

  Dot (graphviz)          : $dot
  Javascript engine       : $JAVASCRIPT
  GConf                   : $gconf
  GtkHtml                 : $gtkhtml

  $IGE_MAC_INTERATION
])

