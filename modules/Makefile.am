EXTRA_DIST = index.xml

incdir = $(libdir)/@PACKAGE@
inc_DATA = index.xml

INCLUDES=-I../src

pkglib_LTLIBRARIES = libmorla_test.la
libmorla_test_la_SOURCES = test.c
libmorla_test_la_LDFLAGS = -module -avoid-version

pkglibdir=$(libdir)/@PACKAGE@

