#include "file_list_tips.h"
#include "gfm_vfs.h"
#include <gtk/gtk.h>
#include <time.h> 
#include "gfm_list_store.h"
#include "interface.h"

#ifndef DEBUG
#define DEBUG 0
#endif 
#include "debug.h"

/* Based on GtkToolTips code */

guint file_list_tips_delay = 500;

static GtkWindow *tip_window;
static GtkLabel *tip_label;
static gint timer_id = 0;
static gboolean tips_enabled = TRUE;

static gint data_panel;
static GtkTreePath *data_row = NULL;
static GtkTreeViewColumn *data_column;

static gchar *render_tip		(gint col,
					const GnomeVFSFileInfo *info, 
					const gchar *rendered_cell);

static void draw_tip_widget 		(gint panel, 
					const gchar *text, 
					gint x, 
					gint y);	

static gboolean show_tip_widget		(gpointer user_data);

static void hide_tip_widget		(void);

static void create_widgets		(void);


static gboolean event_handler		(GtkWidget *widget,
					GdkEvent *event,
					gpointer user_data);
static gboolean tip_event_handler	(GtkWidget *widget,
					GdkEvent *event,
					gpointer user_data);

static gboolean on_leave_notify 	(GtkWidget *widget, 
					GdkEventCrossing *event, 
					gpointer user_data);


void
file_list_tips_install (gint panel) 
{
	GtkObject *flist;
	
	g_return_if_fail (IS_PANEL(panel));
	
#if DEBUG
	puts("Installing tips");
#endif 
	flist = GTK_OBJECT(panels_data[panel].files_view);
#if DEBUG  
	puts("	connecting signals");
#endif 
	g_signal_connect_after (flist, "event-after", (GCallback) event_handler, NULL);
	g_signal_connect (flist, "leave-notify-event", (GCallback) on_leave_notify, NULL);
#if DEBUG  
	puts("done");
#endif 
}


static gchar *render_tip (gint col, const GnomeVFSFileInfo *info, const gchar *rendered_cell)
{
	gchar *text = NULL;

	switch (col) {
	  case GFM_COLUMN_ICON:
		if (info->valid_fields & GNOME_VFS_FILE_INFO_FIELDS_MIME_TYPE)
			text = g_strdup (info->mime_type);
		break;
	  case GFM_COLUMN_NAME:
		if (info->type == GNOME_VFS_FILE_TYPE_SYMBOLIC_LINK && 
	            info->valid_fields & GNOME_VFS_FILE_INFO_FIELDS_SYMLINK_NAME)
			text = g_strdup_printf("%s -> %s", info->name, info->symlink_name);
		else
			text = g_strdup (info->name);
		break;
	  case GFM_COLUMN_SIZE:
		if (info->valid_fields & GNOME_VFS_FILE_INFO_FIELDS_SIZE)
			text = g_strdup_printf ("%'"GNOME_VFS_SIZE_FORMAT_STR " %s", info->size, _("bytes"));
		break;
	  case GFM_COLUMN_MTIME:
		if (info->valid_fields & GNOME_VFS_FILE_INFO_FIELDS_MTIME) {
			struct tm *tm;

			tm = localtime (&info->mtime);
			text = g_new0 (gchar, 100);
			if (text)
				strftime (text, 99, "%c" ,tm);
		}
		break;
	  case GFM_COLUMN_CTIME:
		if (info->valid_fields & GNOME_VFS_FILE_INFO_FIELDS_CTIME) {
			struct tm *tm;

			tm = localtime (&info->ctime);
			text = g_new0 (gchar, 100);
			if (text)
				strftime (text, 99, "%c" ,tm);
		}
		break;
	  case GFM_COLUMN_ATIME:
		if (info->valid_fields & GNOME_VFS_FILE_INFO_FIELDS_ATIME) {
			struct tm *tm;

			tm = localtime (&info->atime);
			text = g_new0 (gchar, 100);
			if (text)
				strftime (text, 99, "%c" ,tm);
		}
		break;
	  case GFM_COLUMN_PERM:
		if (rendered_cell)
			text = g_strdup (rendered_cell);
		break;
	  case GFM_COLUMN_UID:
		text = g_strdup_printf ("%s [%u]",rendered_cell?:"",info->uid);
		break;
	  case GFM_COLUMN_GID:
		text = g_strdup_printf ("%s [%u]",rendered_cell?:"",info->gid);
		break;
	}

	encode_to_utf8 (&text);
	return text?:g_strdup("?");
}


void
file_list_tips_show_tip		(gint panel,
				GtkTreePath *row,
				GtkTreeViewColumn *column,
				guint delay)
{
	if (!tips_enabled) return;

	if (data_row && gtk_tree_path_compare (row,data_row) == 0 && 
	    column == data_column && 
	    (!tip_window || !GTK_WIDGET_VISIBLE(tip_window))) 
	{
		TRACE
		return;
	}

#if DEBUG 
	puts("Entering file_list_tips_show_tip");
#endif 
	
	hide_tip_widget ();
#if DEBUG 
	puts("	tip is hidden");
	puts("	setting callback data");
#endif
	data_panel = panel;
	if (data_row && data_row != row)
		gtk_tree_path_free (data_row);
	data_row = row;
	data_column = column;

#if DEBUG 
	puts("	adding timer");
#endif 
	timer_id = gtk_timeout_add (delay, show_tip_widget, NULL);

#if DEBUG 
	puts("done");
#endif 
}



static gboolean event_handler	(GtkWidget *widget,
				GdkEvent *event,
				gpointer user_data)
{
	GtkTreeView *view;
	GtkTreeModel *model;
	GtkTreeViewColumn *column;
	GtkTreePath *path;
	
	g_return_val_if_fail (GTK_IS_TREE_VIEW (widget), FALSE);
	view = GTK_TREE_VIEW (widget);

	if (event->any.window != gtk_tree_view_get_bin_window (view))
		return FALSE;

	switch (event->type) {
		case GDK_KEY_PRESS:
		case GDK_SCROLL:
			hide_tip_widget ();
			break;


		case GDK_FOCUS_CHANGE: 
			/* FIXME: it seem's like this isn't working */
			if (!event->focus_change.in) {
#if DEBUG 
				puts("FOCUS lost");
#endif 
				hide_tip_widget ();
			}
			break;

		case GDK_MOTION_NOTIFY:


#if DEBUG 
			puts("\nhandling event");
			printf("	-> x: %.1f y: %.1f\n", event->motion.x, event->motion.y);
#endif 
			model = gtk_tree_view_get_model (view);

			if  (!gtk_tree_view_get_path_at_pos (
				view, 
				event->motion.x, event->motion.y, 
				&path, &column, 
				NULL, NULL)) 
			{
				TRACE
				hide_tip_widget ();
				break;
			}
	
	
			file_list_tips_show_tip (get_panel_id (widget), path, column, file_list_tips_delay);

#if DEBUG 
			puts("done");
#endif 
			break;
		default:
			; /* nop */
	}

	return FALSE;
}





static void draw_tip_widget 		(gint panel, const gchar *text, gint x, gint y)
{
	GtkRequisition requisition;
	GtkWidget *widget;
	GtkAdjustment *hadjustment;
	gint w,h, scr_w, scr_h, wx, wy;
#if DEBUG  
	printf("Entering draw_tip_widget(%s,%d,%d)",text,x,y);
#endif
	
	if (!tip_window)
		create_widgets ();
#if DEBUG  
	puts("	setting label");
#endif 
  	gtk_label_set_text (GTK_LABEL (tip_label), text);

#if DEBUG 
	puts("	done, counting position of tip window");
#endif 

	widget = GTK_WIDGET(panels_data[panel].files_view);

	/* we have coords relative to TreeView that is scrolled 
	 * and we need absolute coords */
	g_object_get (G_OBJECT (widget), "hadjustment", &hadjustment, NULL);
	if (hadjustment) 
		x -= hadjustment->value;
	
	gdk_window_get_origin (widget->window, &wx, &wy);
#if DEBUG  
	printf("		origin: [%d,%d]\n", wx, wy);
#endif 
	x+=wx; 
	y+=wy;

	
	scr_w = gdk_screen_width ();
	scr_h = gdk_screen_height ();
#if DEBUG
	printf("		screen: %dx%d\n", scr_w, scr_h);
#endif 

	gtk_widget_size_request (GTK_WIDGET(tip_window), &requisition);
	w = requisition.width;
	h = requisition.height;
#if DEBUG 
	printf("		window_size: %dx%d\n", w, h);
#endif 


	if ((x + w) > scr_w)
	x -= (x + w) - scr_w;
	else if (x < 0)
	x = 0;

#if DEBUG 
	printf("	moving window to [%d, %d]\n",x,y);
#endif 
	gtk_window_move (tip_window, x, y);
#if DEBUG 
	puts("done");
#endif 
}




static void create_widgets	(void)
{
	if (tip_window)
		return;

#if DEBUG 
	puts("creating widgets");
	puts("	window");
#endif 
	tip_window = GTK_WINDOW(gtk_window_new (GTK_WINDOW_POPUP));
	gtk_widget_set_app_paintable (GTK_WIDGET(tip_window), TRUE);
	gtk_window_set_resizable (tip_window, FALSE);
	gtk_widget_set_name (GTK_WIDGET(tip_window), "file-list-tooltips");
	gtk_container_set_border_width (GTK_CONTAINER (tip_window), 4);
	gtk_widget_set_events (GTK_WIDGET(tip_window), 
		GDK_POINTER_MOTION_MASK | GDK_BUTTON_PRESS_MASK | GDK_KEY_PRESS_MASK | GDK_FOCUS_CHANGE_MASK | GDK_SCROLL_MASK);
	g_signal_connect (tip_window, "event", (GCallback) tip_event_handler, NULL);

#if DEBUG
	puts("	label");
#endif 
	tip_label = GTK_LABEL(gtk_label_new (NULL));
	gtk_label_set_line_wrap (tip_label, TRUE);
	gtk_misc_set_alignment (GTK_MISC (tip_label), 0.5, 0.5);
	gtk_widget_show (GTK_WIDGET(tip_label));

	gtk_container_add (GTK_CONTAINER (tip_window), GTK_WIDGET(tip_label));
#if DEBUG 
	puts("done");
#endif 
}




static void hide_tip_widget		(void)
{
	if (!tip_window)
		return;

#if DEBUG 
	puts("Entering hide_tip_widget");
#endif

	if (data_row)
		gtk_tree_path_free (data_row);
	data_row = NULL;
	data_column = NULL;
	
	if (GTK_WIDGET_VISIBLE(tip_window))
		gtk_widget_hide (GTK_WIDGET(tip_window));

#if DEBUG 
	puts("	widget hidden, removing timer");
#endif 
	
	if (timer_id) {
		gtk_timeout_remove (timer_id);
		timer_id = 0;
	}
#if DEBUG 
	puts("done");
#endif
}




static gboolean show_tip_widget		(gpointer user_data)
{
	GdkRectangle rect;
	gint col;
	gchar *text=NULL;
	GnomeVFSFileInfo *info;
	GfmRowData *data;
	GtkTreeIter iter;
	GtkTreeModel *model;
	
	g_return_val_if_fail (IS_PANEL(data_panel),FALSE);
	g_return_val_if_fail (GTK_IS_TREE_VIEW_COLUMN (data_column),FALSE);
	
#if DEBUG
	puts("entering show_tip_widget");
#endif 
	model = GTK_TREE_MODEL (panels_data[data_panel].files_data);
	g_return_val_if_fail (gtk_tree_model_get_iter (model, &iter, data_row), FALSE);

	col = gtk_tree_view_column_get_sort_column_id (data_column);

	gtk_tree_model_get (model, &iter,
			GFM_COLUMN_DATA, &data,
			col, &text,
			-1
	);
	info = data->info;
	
#if DEBUG 
	printf("	column: %d\n", col);
#endif 

	text = render_tip (col, info, text);
#if DEBUG 
	printf("	text: %s\n", text?:"?");
#endif 

	g_return_val_if_fail (panels_data[data_panel].files_view != NULL, FALSE);
	g_return_val_if_fail (GTK_IS_TREE_VIEW(panels_data[data_panel].files_view),FALSE);
	gtk_tree_view_get_cell_area     (
		panels_data[data_panel].files_view,
        	data_row,
		data_column,
		&rect);
	

	draw_tip_widget (data_panel, text?:"?", rect.x, rect.y+rect.height+2);	

#if DEBUG 
	puts("	releasing text");
#endif 
	if (text)
		g_free (text);
#if DEBUG  
puts("done");
#endif 

	if (!GTK_WIDGET_VISIBLE (GTK_WIDGET(tip_window)))
		gtk_widget_show (GTK_WIDGET(tip_window));
	return FALSE;
}

void
file_list_tips_enable		(void)
{
	tips_enabled = TRUE;
}

void
file_list_tips_disable		(void)
{
	tips_enabled = FALSE;
	hide_tip_widget ();
}

void
file_list_tips_hide_tip		(void)
{
	hide_tip_widget ();
}

static gboolean tip_event_handler	(GtkWidget *widget,
					GdkEvent *event,
					gpointer user_data)
{
	switch (event->type) {
		case GDK_KEY_PRESS:
		case GDK_BUTTON_PRESS:
		case GDK_SCROLL:
		case GDK_FOCUS_CHANGE: 
		case GDK_MOTION_NOTIFY:
			hide_tip_widget ();
			break;
		default:
			; /* nop */
	}

	return FALSE;

}

static gboolean
on_leave_notify (GtkWidget *widget, GdkEventCrossing *event, gpointer user_data)
{
#if DEBUG  
	puts("leave_notify");
#endif 
	if (tip_window && !GTK_WIDGET_VISIBLE (tip_window))
		hide_tip_widget ();

	return FALSE;
}
