#include "interface.h"

extern guint file_list_tips_delay;

void
file_list_tips_install		(gint panel);

void
file_list_tips_show_tip		(gint panel,
				GtkTreePath *row,
				GtkTreeViewColumn *column,
				guint delay);
void
file_list_tips_hide_tip		(void);

/* default: tips are enabled */
void
file_list_tips_enable		(void);

void
file_list_tips_disable		(void);
