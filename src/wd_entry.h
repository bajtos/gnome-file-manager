#include <gnome.h>

gboolean
on_wd_entry_key_press_event           (GtkWidget       *widget,
					GdkEventKey     *event,
					gpointer         user_data);

void 
wd_entry_set_text		      (gint panel);


/*
 * event handlers
 */

void
on_wd_entry_realize                    (GtkWidget       *widget,
                                        gpointer         user_data);
void
on_wd_entry_destroy                    (GtkObject       *object,
                                        gpointer         user_data);

