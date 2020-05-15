bin_PROGRAMS = morla

INCLUDES = \
	-DPATH_IMAGE=\"$(datadir)/@PACKAGE@/bootsplash.png\" \
	-DPATH_ICON=\"$(datadir)/@PACKAGE@/icons/64_color.png\" \
	-DPATH_MODULE=\"$(libdir)/@PACKAGE@\"

EXTRA_DIST = editor.h morla.h morla-module.h

morla_SOURCES = \
	about.c \
	blanknode.c \
	browser.c \
	checks.c \
	datatype.c \
	dialog.c \
	download.c \
	edit.c \
	file.c \
	filechooser.c \
	gconf.c \
	graph.c \
	help.c \
	info.c \
	init.c \
	js.c \
	last.c \
	main.c \
	maker.c \
	merge.c \
	module.c \
	namespace.c \
	navigator.c \
	ntriples.c \
	preferences.c \
	query.c \
	rdf.c \
	rdfs.c \
	search.c \
	splash.c \
	template.c \
	textview.c \
	unredo.c \
	update.c

incdir = $(includedir)
inc_DATA = morla-module.h

