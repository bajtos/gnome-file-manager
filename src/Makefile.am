## Process this file with automake to produce Makefile.in
NULL=
INCLUDES = \
	-Wall -Wmissing-prototypes \
	$(GFM_CFLAGS) \
	-DPACKAGE=\""$(PACKAGE)"\" \
	-DPREFIX=\""$(prefix)"\" \
	-DDATADIR=\""$(datadir)"\" \
	-DGFM_DATADIR=\""$(pkgdatadir)"\" \
	-DPACKAGE_LOCALE_DIR=\""$(pkgdatadir)/locale"\" \
	-DGFM_PIXMAPDIR=\""$(datadir)/pixmaps/$(PACKAGE)"\" \
	-DVERSION=\""$(VERSION)"\" \
	$(NULL)

##	-DGDK_DISABLE_DEPRECATED \
##	-DGTK_DISABLE_DEPRECATED \
##	-DGDK_PIXBUF_DISABLE_DEPRECATED \
##	-DGNOME_DISABLE_DEPRECATED \
	   
gfm_LDADD = $(GFM_LIBS)
gfm_LDFLAGS = -export-dynamic	# for libglade

bin_PROGRAMS = gfm

gfm_SOURCES = \
	main.c \
	interface.c interface.h \
	gfm_list_store.c gfm_list_store.h \
	wd_entry.c wd_entry.h \
	gfm_vfs.c gfm_vfs.h \
	file_list_tips.c file_list_tips.h \
	mime_actions.c mime_actions.h \
	special_files.c special_files.h \
	special_files_symlinks.c special_files_symlinks.h \
	file_list_interface.c file_list_interface.h\
	file_list.c file_list.h \
	callbacks.c \
	debug.h \
	file_list_icons.c file_list_icons.h \
	copy_move_delete.c copy_move_delete.h \
	configuration.c configuration.h \
	selections.c selections.h \
	lister.c lister.h \
	preferences.c preferences.h \
	$(NULL)

uidir = $(pkgdatadir)/ui
ui_DATA = gfm.glade2

EXTRA_DIST = $(ui_DATA)

