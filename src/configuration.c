#ifndef DEBUG
#define DEBUG 0
#endif
#include "debug.h"

#include "configuration.h"
#include <gconf/gconf-client.h>
#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <gnome.h>
#include "gfm_vfs.h"
#include "file_list.h"
#include "gfm_list_store.h"

#define USE_DEPRECATED 0
char *panel_names[NUM_PANELS] = {"panel-left/","panel-right/"};

/*
 * we use only one instance of GConfClient. this function creates instance (or returns already created one)
 * plus sets up destroy during shutdown
 */
static GConfClient *
get_gconf_client (void);


void
configuration_wd_save (GnomeEntry *gentry)
{
	GConfClient *client = get_gconf_client();
	gchar *key;
	const gchar *value;
	gint panel;

	
	g_return_if_fail (gentry != NULL);
	panel = get_panel_id (gentry);
	g_return_if_fail (panels_data[panel].uri_wdir != NULL);

	/*
	 * we can also store history in future, therefore we use separate GConf dir
	 */
	key = g_strconcat (CONFIGURATION_BASE , panel_names[panel], "wd-entry/value", NULL);
//	value = gtk_entry_get_text (GTK_ENTRY(gnome_entry_gtk_entry (gentry)));
TRACE
	value = gnome_vfs_uri_to_string (panels_data[panel].uri_wdir, GNOME_VFS_URI_HIDE_NONE);
TRACE

	if (value && key)
	g_return_if_fail (client != NULL);
		if (!
		gconf_client_set_string (client, key, value, NULL)
		) fprintf(stderr,"%s not saved !\n", key);
	if (key)
		g_free (key);

}	

void
configuration_wd_load (GnomeEntry *gentry)
{
	GConfClient *client = get_gconf_client ();
	gchar *key, *value;

	g_return_if_fail (gentry != NULL);
	g_return_if_fail (client != NULL);

	key = g_strconcat (CONFIGURATION_BASE, panel_names[get_panel_id (gentry)], "wd-entry/value", NULL);
	
	if (key) {
		value = gconf_client_get_string (client, key, NULL);
		if (!value || *value=='\0')
			value = g_get_current_dir ();
		
		if (value) {
			GnomeVFSURI *uri;
			uri = gnome_vfs_uri_new (value);
			
			if (uri) {
				file_list_chdir (
					get_panel_id (gentry),
					uri);
				gnome_vfs_uri_unref (uri);
			}
			free (value);
		}
		free (key);
	}
}

/*
 * main window: well, good window manager should store our position&size between sessions, but most of non-gnome-compliant
 * ones don't. I had some problems with (re)storing position, so I save/load size only.
 */

void
configuration_main_win_load (GtkWindow *window)
{
	GConfClient *client = get_gconf_client ();
	gchar *key;

	gint w=0, h=0;
//	gint x, y;

	g_return_if_fail (window != NULL);
	g_return_if_fail (client != NULL);

	TRACEM("Entering configuration_main_win_load");

	key = g_strconcat (CONFIGURATION_BASE,  "main-window/size", NULL);
	TRACEM("	key: %s\n",key);

	if (key) {
		if (gconf_client_get_pair (client, key, GCONF_VALUE_INT, GCONF_VALUE_INT, &w, &h, NULL) && w && h) {
			TRACEM("	Setting main_window size to %dx%d\n",w,h);
			gtk_window_set_default_size (window, w, h);
		}
		free (key);
	}		

/*
	key = g_strconcat (CONFIGURATION_BASE, gtk_widget_get_name (GTK_WIDGET(window)), "-position", NULL);
	if (key) {
		gconf_client_get_pair (client, key, GCONF_VALUE_INT, GCONF_VALUE_INT, &x, &y, NULL);
		free (key);
		if (x && y)
			gdk_window_move(
				GTK_WIDGET(window)->window,
				x, y);
	}		
*/
	TRACEM("Leaving configuration_main_win_load");

}

void
configuration_main_win_store (GtkWindow *window)
{
	GConfClient *client = get_gconf_client ();
	gchar *key;
	gint w, h;
//	gint x,y, d;

	g_return_if_fail (window != NULL);
	g_return_if_fail (client != NULL);


/*	gdk_window_get_geometry (
		GTK_WIDGET(window)->window,
		&x, &y,
		&w, &h,
		&d);
*/
//	x = GPOINTER_TO_INT (gtk_object_get_data (GTK_OBJECT(window), "x"));
//	y = GPOINTER_TO_INT (gtk_object_get_data (GTK_OBJECT(window), "y"));
	w = GPOINTER_TO_INT (g_object_get_data (G_OBJECT(window), "w"));
	h = GPOINTER_TO_INT (g_object_get_data (G_OBJECT(window), "h"));

	
	key = g_strconcat (CONFIGURATION_BASE,  "main-window/size", NULL);
	if (key) {
		if (!
		gconf_client_set_pair (client, key, GCONF_VALUE_INT, GCONF_VALUE_INT, &w, &h, NULL)
		) fprintf(stderr,_("%s not saved !\n"), key);
		free (key);
	}		

/*
	key = g_strconcat (CONFIGURATION_BASE, gtk_widget_get_name (GTK_WIDGET(window)), "-position", NULL);
	if (key) {
		gconf_client_set_pair (client, key, GCONF_VALUE_INT, GCONF_VALUE_INT, &x, &y, NULL);
		free (key);
	}		
*/
}




void
configuration_paned_load (GtkPaned *paned)
{
	GConfClient *client = get_gconf_client ();
	gchar *key;
	gint pos;


	g_return_if_fail (paned != NULL);
	g_return_if_fail (client != NULL);

	key = g_strconcat (CONFIGURATION_BASE, "paned/position", NULL);

	if (!key) return;
	pos = gconf_client_get_int (client, key, NULL);

	free (key);

	if (!pos) return;

	gtk_paned_set_position (paned, pos);

}

void
configuration_paned_store (GtkPaned *paned)
{
	GConfClient *client = get_gconf_client ();
	gchar *key;

	g_return_if_fail (paned != NULL);
	g_return_if_fail (client != NULL);

	key = g_strconcat (CONFIGURATION_BASE, "paned/position", NULL);

	if (!key) return;

		if (!
	gconf_client_set_int (client, key, paned->position_set ? paned->child1_size : 0, NULL)
		) fprintf(stderr,_("%s not saved !\n"), key);

	free (key);

}


void
configuration_show_hidden_files_load (GtkCheckMenuItem *item)
{
	GConfClient *client = get_gconf_client();
	gchar *key;
	GError *error = NULL;
	gboolean value;

	g_return_if_fail (item != NULL);
	g_return_if_fail (client != NULL);
TRACE
	key = g_strconcat (CONFIGURATION_BASE, "misc/show-hidden-files", NULL);
	if (!key) {
		g_warning (G_STRLOC ": could not create key\n");
		return;
	}

	value = gconf_client_get_bool (client, key, &error);
	
	if (error) {
		g_warning (G_STRLOC ": could not load configuration (%s)", error->message);
		return;
	}

	gtk_check_menu_item_set_active (item, value);
}

void
configuration_show_hidden_files_store (GtkCheckMenuItem *item)
{
	GConfClient *client = get_gconf_client();
	gchar *key;
	GError *error = NULL;
	gboolean value;

	g_return_if_fail (item != NULL);
	g_return_if_fail (client != NULL);

TRACE
	value = gtk_check_menu_item_get_active (item);

	key = g_strconcat (CONFIGURATION_BASE, "misc/show-hidden-files", NULL);
	if (!key) {
		g_warning (G_STRLOC ": could not create key\n");
		return;
	}

	gconf_client_set_bool (client, key, value, &error);
	
	if (error) {
		g_warning (G_STRLOC ": could not store configuration (%s)", error->message);
		return;
	}

TRACE
}

static GConfClient *
get_gconf_client (void) 
{
	static GConfClient *global_gconf_client = NULL;

	if (!global_gconf_client) {
		global_gconf_client = gconf_client_get_default();
		g_return_val_if_fail (global_gconf_client != NULL, NULL);
		
		/* destroy global_gconf_client in final cleanup */
		g_object_set_data_full (G_OBJECT(main_window), "my-gconf-client", global_gconf_client, g_object_unref);
	}
	return global_gconf_client;
}

