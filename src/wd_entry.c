#include <gdk/gdkkeysyms.h>
#include <string.h>
#include "gfm_vfs.h"
#include "wd_entry.h"
#include "file_list.h"
#include "interface.h"
#include "configuration.h"

#ifndef DEBUG
#define DEBUG 0
#endif 
#include "debug.h"

gboolean
on_wd_entry_key_press_event           (GtkWidget       *widget,
                                        GdkEventKey     *event,
                                       gpointer         user_data)
{
	GnomeVFSURI *uri, *old_uri;
	GtkEntry *entry;
	gchar *path;
	gint panel = get_panel_id (widget);
	
	switch (event->keyval) {
		case GDK_Return:
			entry = GTK_ENTRY (widget);
			g_return_val_if_fail (entry != NULL, FALSE);

			path = 	gnome_vfs_expand_initial_tilde (
					gtk_entry_get_text (entry));
#if DEBUG 
printf("RETURN captured (%s), path=%s\n",gtk_widget_get_name(widget), path);
#endif 
			if (path) {
				uri = gnome_vfs_uri_new (path);
				if (gnome_vfs_uri_get_password(uri)) {
					/* hide password */
					g_free (path);
					path = gnome_vfs_uri_to_string (uri, GNOME_VFS_URI_HIDE_PASSWORD);
				}

				if (!uri) {
					report_gnome_vfs_error (GNOME_VFS_ERROR_INVALID_URI, TRUE);
					break;
				}

TRACE
				old_uri = panels_data[panel].uri_wdir;
				/* set (hidden) password from old_uri */
#define EQUAL(WHAT) !strcmp (gnome_vfs_uri_get_##WHAT (uri)?:"", gnome_vfs_uri_get_##WHAT (old_uri)?:"")
				if (old_uri && 
				    !GFM_URI_IS_LOCAL (old_uri) &&
				    strcmp (gnome_vfs_uri_get_password (old_uri)?:"", "") &&
				    !strcmp (gnome_vfs_uri_get_password (uri)?:"", "") &&
				    
				    EQUAL (scheme) &&
				    EQUAL (host_name) &&
				    gnome_vfs_uri_get_host_port(uri) == gnome_vfs_uri_get_host_port (old_uri) &&
				    EQUAL (user_name)
				)
#undef EQUAL
				{
TRACE				
					gnome_vfs_uri_set_password (uri, gnome_vfs_uri_get_password (old_uri));
				}
TRACEM("real uri: %s\n", gnome_vfs_uri_to_string (uri, 0));
				gtk_entry_set_text (GTK_ENTRY(widget), path);
				file_list_chdir (panel, uri);

				gnome_vfs_uri_unref (uri);
				g_free (path);
			}
			/* TODO: 
			else 
				report out of memory ? 
			*/
			return FALSE; /* so GnomeEntry can store this value into history */
/*
		case GDK_Tab:
			break;
*/
		case GDK_Escape:
			gtk_widget_grab_focus (GTK_WIDGET (panels_data[panel].files_view));
			break;
		default:
			return FALSE;
	}

	return TRUE;
}	

void 
wd_entry_set_text		      (gint panel)
{
	gchar *text;
	GnomeVFSURI *uri;
	GnomeVFSURIHideOptions opts;
	
	g_return_if_fail (IS_PANEL(panel));
	g_return_if_fail ((uri = panels_data[panel].uri_wdir));
	
	opts = GNOME_VFS_URI_HIDE_NONE | GNOME_VFS_URI_HIDE_PASSWORD;
	if (GFM_URI_IS_LOCAL (uri))
		opts |= GNOME_VFS_URI_HIDE_TOPLEVEL_METHOD;
	
	g_return_if_fail((text = gnome_vfs_uri_to_string (uri, opts))); 
/* we can't use following code, since it can mangle URI
BUT we can unescape some special characters like space

	text2 = gnome_vfs_unescape_string_for_display (text);
	free (text);
	text = text2;
*/
	
	gtk_entry_set_text (GTK_ENTRY(gnome_entry_gtk_entry(panels_data[panel].wdir_entry)), text);
	free(text);
	
}

void
on_wd_entry_realize                    (GtkWidget       *widget,
                                        gpointer         user_data)
{
	const gchar *name = gtk_widget_get_name (widget);
	int panel = (name[0] == 'l')?0:1;
	
	panels_data [panel].wdir_entry = GNOME_ENTRY(widget);
	configuration_wd_load (GNOME_ENTRY (widget));
}


void
on_wd_entry_destroy                    (GtkObject       *object,
                                        gpointer         user_data)
{
	g_return_if_fail (GNOME_IS_ENTRY (object));
	configuration_wd_save (GNOME_ENTRY(object));
}

