## Process this file with automake to produce Makefile.in

SUBDIRS = po src testing pixmaps help

appicondir = $(datadir)/pixmaps
appicon_DATA = pixmaps/gfm.png

@INTLTOOL_DESKTOP_RULE@
Utilitiesdir = $(datadir)/applications
Utilities_in_files = gnome-file-manager.desktop.in
Utilities_DATA = $(Utilities_in_files:.desktop.in=.desktop)

EXTRA_DIST = \
	xmldocs.make\
	$(Utilities_in_files)\
	$(appicon_DATA)

