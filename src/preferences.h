#include <gtk/gtk.h>
/* prepare and run 'Preferences' dialog */
void
preferences_run	(void);




/*
 * stuff for libglade 
 */
GtkWidget *
create_view_current 		(gchar *widget_name, 
				gchar *unused_string1, 
				gchar *unused_string2,
				gint unused_int1, 
				gint unused_int2);	
GtkWidget *
create_view_avail 		(gchar *widget_name, 
				gchar *unused_string1, 
				gchar *unused_string2,
				gint unused_int1, 
				gint unused_int2);	

/*
 * event handling
 */
void
on_option_panel_changed		(GtkOptionMenu *optionmenu, 
				gpointer data);

gboolean
on_arrow_up_clicked		(GtkArrow *arrow,
				GdkEventButton *event,
				gpointer user_data);
gboolean
on_arrow_left_clicked		(GtkArrow *arrow,
				GdkEventButton *event,
				gpointer user_data);
gboolean
on_arrow_right_clicked		(GtkArrow *arrow,
				GdkEventButton *event,
				gpointer user_data);
gboolean
on_arrow_down_clicked		(GtkArrow *arrow,
				GdkEventButton *event,
				gpointer user_data);

