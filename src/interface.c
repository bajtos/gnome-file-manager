#include "interface.h"
#include <glade/glade.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#ifdef HAVE_CONFIG_H
  #include <config.h>
#endif 
  
#ifndef DEBUG
#define DEBUG 0
#endif
#include "debug.h"

PanelData panels_data[NUM_PANELS];
GtkWidget *main_window = NULL;
GnomeAppBar *app_bar = NULL;
GtkTooltips *tooltips = NULL;

static gboolean
free_xml_data (GladeXML *data)
{
	g_object_unref (data);
	return 0;
}

/** libglade stuff **/
#if 0
static GladeXML *xml_data = NULL;


static void 
init_xml_data (void)
{
	if (xml_data) return;
	
	xml_data = glade_xml_new ("gfm.glade",NULL,NULL);
	/* TODO: add translation domain ?! */
	glade_xml_signal_autoconnect (xml_data);
	gtk_quit_add (1, (GtkFunction) free_xml_data ,xml_data);
}
#endif

/** main window **/
void
create_main_window () 
{
	GdkPixbuf *icon = NULL;
	GError *error = NULL;

	main_window = create_widget ("main_window");
	g_assert (main_window != NULL);

	app_bar = GNOME_APPBAR (lookup_widget (main_window, "appbar"));
	g_assert (app_bar != NULL);

	tooltips = gtk_tooltips_new ();

	TRACE
	icon = gdk_pixbuf_new_from_file (DATADIR "/pixmaps/gfm.png", &error);
	TRACE
	if (icon) {
		TRACE
		gtk_window_set_icon (GTK_WINDOW(main_window), icon);
		g_object_unref (G_OBJECT(icon));
	} else {
		TRACE
		g_warning (G_STRLOC ": could not load icon -- %s", error->message);
		g_error_free (error);
	}


	gtk_widget_show (main_window);
}


GtkWidget *
create_widget (gchar *dialog_name)
{
	GtkWidget *widget;
	GladeXML *xml;
	
	xml = glade_xml_new (GFM_DATADIR "/ui/gfm.glade2", dialog_name, NULL);
	g_return_val_if_fail (xml != NULL, NULL);
	
	widget = glade_xml_get_widget (xml, dialog_name);
	g_return_val_if_fail (widget != NULL, NULL);
	glade_xml_signal_autoconnect (xml);

//	g_signal_emit_by_name (widget, "realize", NULL);
//	gtk_widget_realize (widget);
	
	gtk_quit_add (gtk_main_level(), (GtkFunction) free_xml_data ,xml);
	gtk_quit_add_destroy (gtk_main_level(), GTK_OBJECT(widget));

	return widget;
}
	
GtkWidget *
lookup_widget (GtkWidget *relative, gchar *name)
{
	GladeXML *xml;
	GtkWidget *widget;

	xml = glade_get_widget_tree (relative);
	g_return_val_if_fail (xml != NULL, NULL);

	widget = glade_xml_get_widget (xml, name);

	g_return_val_if_fail (widget != NULL, NULL);

	return widget;
}

void
encode_to_utf8	(gchar **text)
{
	gchar *text2;
	GError *error = NULL;
	
	if (!text || !*text) return;
	text2 = g_locale_to_utf8 (*text, -1, NULL, NULL, &error); 

	if (error) 
		g_warning (_("Could not convert data to UTF-8: %s\n"), error->message);
	
	free (*text);
	*text = text2;
}
	
	
