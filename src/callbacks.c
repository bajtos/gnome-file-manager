#ifndef DEBUG
#define DEBUG 0
#endif
#include "debug.h"

#include <gnome.h>

#include "interface.h"
#include "configuration.h"

#include <stdlib.h>
#include <glib.h>
#include <gdk/gdkkeysyms.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include "gfm_vfs.h"
#include "preferences.h"
#include "file_list.h"
#include "copy_move_delete.h"


/*
 * main-menu items
 */
void
on_preferences_activate			(GtkMenuItem     *menuitem,
					gpointer         user_data);
void
on_help_activate			(GtkMenuItem     *menuitem,
					gpointer         user_data);
void
on_about_activate			(GtkMenuItem     *menuitem,
					gpointer         user_data);
void
on_show_hidden_files_toggled		(GtkCheckMenuItem *item, 
					gpointer user_data);
/*
 * hooks for loading&saving of current state (configuration)
 */
void
on_show_hidden_files_realize		(GtkWidget *item,
					gpointer user_data);
void
on_show_hidden_files_destroy		(GtkWidget *item,
					gpointer user_data);
void
on_main_window_realize			(GtkWidget       *widget,
					gpointer         user_data);
void
on_exit_activate			(GtkMenuItem     *menuitem,
					gpointer         user_data);
void
on_df_realize              	     (GtkWidget       *widget,
                                	 gpointer         user_data);
void
on_file_info_realize                   (GtkWidget       *widget,
                                        gpointer         user_data);
void
on_hpaned_realize                      (GtkWidget       *widget,
                                        gpointer         user_data);
void
on_hpaned_destroy                      (GtkObject       *object,
                                        gpointer         user_data);
void
on_main_window_size_allocate           (GtkWidget       *widget,
                                        GtkAllocation   *allocation,
                                        gpointer         user_data);

/*
 * application shutdown
 */
void
on_main_window_destroy                 (GtkObject       *object,
                                        gpointer         user_data);

/*
gboolean
on_file_list_focus_in_event		(GtkWidget       *widget,
					GdkEventFocus   *event,
					gpointer         user_data);
*/

/*************************************
 *
 * 	CODE
 *
 *************************************/

void
on_preferences_activate               (GtkMenuItem     *menuitem,
                                       gpointer         user_data)
{
	preferences_run();
}


void
on_help_activate                     (GtkMenuItem     *menuitem,
                                       gpointer         user_data)
{
	GError *error = NULL;
	
	if (!gnome_help_display (PACKAGE_NAME, NULL, &error)) 
		g_warning ("gnome_help_display failed: %s", error->message);

}
void
on_about_activate                     (GtkMenuItem     *menuitem,
                                       gpointer         user_data)
{
	static GtkWidget *about_dialog = NULL;
	gchar *authors[] = {"Miroslav Bajtoš", NULL};
	gchar *documenters[] = {NULL};
	gchar *file;
	GError *error = NULL;
	GdkPixbuf *pixbuf;
	
	if (about_dialog) {
		gdk_window_show (about_dialog->window);
		gdk_window_raise (about_dialog->window);
		return;
	}

	file = gnome_program_locate_file (NULL, GNOME_FILE_DOMAIN_APP_PIXMAP, PACKAGE"/gfm-logo.png", FALSE, NULL);
	g_return_if_fail (file != NULL);
	pixbuf = gdk_pixbuf_new_from_file (file, &error);
	g_free (file);
	g_return_if_fail (pixbuf != NULL);

	if (error) {
		g_warning (G_STRLOC ": cannot open %s: %s", file, error->message);
		g_error_free (error);
	}
	
	about_dialog = gnome_about_new (
			_("Gnome File Manager"),
			VERSION,
			"(c) 2002 Miroslav Bajtoš", /* copyright */
			_("Uncredible file manager for Gnome desktop"), /* comments */
			(const char **) authors,
			(const char **) documenters,
			_("Default locale created by me ;-)"),
			pixbuf);
	
	if (pixbuf)
		g_object_unref (G_OBJECT(pixbuf));

	if (!about_dialog) {
		g_warning (G_STRLOC ": cannot create about dialog!");
		return;
	}

	gtk_window_set_transient_for (GTK_WINDOW (about_dialog), GTK_WINDOW (main_window));
	g_signal_connect(G_OBJECT(about_dialog), "destroy",
		GTK_SIGNAL_FUNC(gtk_widget_destroyed), &about_dialog);
	gtk_widget_show (about_dialog);
}

void
on_show_hidden_files_toggled 		(GtkCheckMenuItem *item, 
					gpointer user_data)
{

	file_list_toggle_hidden_files (PANEL_LEFT);
	file_list_toggle_hidden_files (PANEL_RIGHT);
}

void
on_show_hidden_files_realize		(GtkWidget *item,
					gpointer user_data)
{
	configuration_show_hidden_files_load (GTK_CHECK_MENU_ITEM (item));
}

void
on_show_hidden_files_destroy		(GtkWidget *item,
					gpointer user_data)
{
	configuration_show_hidden_files_store (GTK_CHECK_MENU_ITEM (item));
}

/*
gboolean
on_file_list_focus_in_event            (GtkWidget       *widget,
                                        GdkEventFocus   *event,
                                        gpointer         user_data)
{

  return FALSE;
}
*/




void
on_main_window_realize                 (GtkWidget       *widget,
                                        gpointer         user_data)
{
	configuration_main_win_load (GTK_WINDOW(widget));
}


void
on_exit_activate                       (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	gtk_widget_destroy (main_window);
}



void
on_df_realize                   (GtkWidget       *widget,
                                 gpointer         user_data)
{
	int panel = get_panel_id (widget);
	
	panels_data [panel].dfree = GTK_LABEL(widget);
}

void
on_file_info_realize                   (GtkWidget       *widget,
                                        gpointer         user_data)
{
	int panel = get_panel_id (widget);
	
	panels_data [panel].finfo = GTK_LABEL(widget);
}


void
on_hpaned_realize                      (GtkWidget       *widget,
                                        gpointer         user_data)
{
	configuration_paned_load (GTK_PANED(widget));
}


void
on_hpaned_destroy                      (GtkObject       *object,
                                        gpointer         user_data)
{
	configuration_paned_store (GTK_PANED(object));
}




void
on_main_window_size_allocate           (GtkWidget       *widget,
                                        GtkAllocation   *allocation,
                                        gpointer         user_data)
{
	gint w, h;
	
/*
printf("x: %d y: %d w: %d h: %d\n", allocation->x, allocation->y, allocation->width, allocation->height);
	if (! widget->window) return;

	gdk_window_get_size (
		widget->window,
		&w, &h);
*/
	if (!widget->window) return;

	w = allocation->width;
	h = allocation->height;

	if (! (h && w)) return;

	/*
	 * we only "remember" our size, it's saved at shutdown
	 */
	g_object_set_data (G_OBJECT (widget), "w", GINT_TO_POINTER(w));
	g_object_set_data (G_OBJECT (widget), "h", GINT_TO_POINTER(h));
}


/*
 * application shutdown
 */
void
on_main_window_destroy                 (GtkObject       *object,
                                        gpointer         user_data)
{
	TRACEM("Entering main_window_destroy");
	
	configuration_main_win_store (GTK_WINDOW(object));
	TRACEM("	configuration stored");

	copy_move_delete_shutdown ();
	TRACEM("copy_move_delete shot down");
/*
  	gnome_vfs_shutdown();
	TRACEM("gnome_vfs shot down");
*/

	TRACEM("quitting...");
	gtk_main_quit ();
	TRACEM("Leaving main_window_destroy");
}
