#ifndef __INTERFACE_H__
#define __INTERFACE_H__


#include <gnome.h>
#include <gtk/gtk.h>
#include <libgnomevfs/gnome-vfs.h>

struct _PanelData {
	GnomeEntry* wdir_entry;		/* gnome-entry showing working directory */
	GtkLabel *dfree;		/* free space left in device */
	GtkLabel *finfo;		/* info about select file(s) */
	GtkListStore *files_data;	/* data of each file listed */
	GtkTreeView *files_view;	/* file list */
	GnomeVFSAsyncHandle **load_dir_handle;
					/* handle for async. loading of files in actual directory */
	
	GnomeVFSURI *uri_wdir;		/* uri of working directory */
	GnomeVFSURI *uri_ndir;		/* uri of directory where are we switching to */
	/* we need these two URIs for proper handling of user-abort during loading new directory */
	
	gboolean has_hidden_files;
};

typedef struct _PanelData PanelData;

enum {
	PANEL_LEFT,
	PANEL_RIGHT,
	NUM_PANELS
};

extern PanelData panels_data[NUM_PANELS];


extern GtkWidget *main_window;
extern GnomeAppBar *app_bar;
extern GtkTooltips *tooltips;

#define FILE_ICON_SIZE 12

void
create_main_window (void);

GtkWidget *
create_widget (gchar *dialog_name);

/*
 * returns widget named 'name' from the window containing 'relative'
 */
GtkWidget *
lookup_widget (GtkWidget *relative, gchar *name);

#define get_panel_id(W) \
	(gtk_widget_get_name (GTK_WIDGET(W))[0]=='l' ? 0 :\
	 gtk_widget_get_name (GTK_WIDGET(W))[0]=='r' ? 1 :\
	 -1)

#define IS_PANEL(p) \
	(p == PANEL_LEFT || p ==PANEL_RIGHT)

#define OTHER_PANEL(p)\
	(p == PANEL_LEFT ? PANEL_RIGHT : PANEL_LEFT)
void
encode_to_utf8	(gchar **text);
	
#endif /* __INTERFACE_H__ */
