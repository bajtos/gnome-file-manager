#include <gtk/gtk.h>

/* used when gray '+'/'-'/'*' is pressed */
void
selections_select_all 		(GtkTreeView *view);

void 
selections_unselect_all		(GtkTreeView *view);

void
selections_invert_selection	(GtkTreeView *view);

/* install callback to monitor selection and show 'selection info' */
void
selections_install_monitor	(GtkTreeView *view);
