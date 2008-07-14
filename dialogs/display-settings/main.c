/*
 *  Copyright (c) 2008 Nick Schermer <nick@xfce.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <glib.h>
#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <glade/glade.h>

#include <libxfcegui4/libxfcegui4.h>
#include <libxfce4util/libxfce4util.h>
#include <xfconf/xfconf.h>

#include <X11/extensions/randr.h>
#include <X11/extensions/Xrandr.h>
#include <X11/extensions/xf86vmode.h>

#include "display-dialog_glade.h"



#define IS_STRING(string) (string != NULL && *string != '\0')



enum
{
    COLUMN_MONITOR_NAME,
    COLUMN_MONITOR_ICON,
    COLUMN_MONITOR_ID,
    N_MONITOR_COLUMNS
};

enum
{
    COLUMN_COMBO_NAME,
    COLUMN_COMBO_VALUE,
    N_COMBO_COLUMNS
};



/* xrandr rotation name conversion */
typedef struct {
  Rotation     rotation;
  const gchar *name;
} RotationTypes;

static const RotationTypes rotation_names[] = 
{
  { RR_Rotate_0, N_("Normal") },
  { RR_Rotate_90, N_("Left") },
  { RR_Rotate_180, N_("Inverted") },
  { RR_Rotate_270, N_("Right") }
};



/* option entries */
static gboolean opt_version = FALSE;
static GOptionEntry option_entries[] =
{
    { "version", 'v', 0, G_OPTION_ARG_NONE, &opt_version, N_("Version information"), NULL },
    { NULL }
};

/* global xfconf channel */
static XfconfChannel *display_channel;

/* active xrandr configuration */
XRRScreenConfiguration *screen_config = NULL;



static void
display_setting_populate_rotation (GladeXML *gxml)
{
    GtkTreeModel *model;
    GtkWidget    *combobox;
    Rotation      rotations;
    Rotation      current_rotation;
    guint         active, i;
    GtkTreeIter   iter;

    /* get the combo box store and clear it */
    combobox = glade_xml_get_widget (gxml, "screen-rotation");
    model = gtk_combo_box_get_model (GTK_COMBO_BOX (combobox));
    gtk_list_store_clear (GTK_LIST_STORE (model));

    /* get the active and possible rotations */
    rotations = XRRConfigRotations (screen_config, &current_rotation);

    /* test and append rotations */
    for (active = i = 0; i < G_N_ELEMENTS (rotation_names); i++)
    {
        if ((rotations & rotation_names[i].rotation) != 0)
        {
          /* insert in store */
          gtk_list_store_insert_with_values (GTK_LIST_STORE (model), &iter, i, COLUMN_COMBO_NAME, _(rotation_names[i].name), 
                                             COLUMN_COMBO_VALUE, rotation_names[i].rotation, -1);

          /* get active rotation */
          if (rotation_names[i].rotation == current_rotation)
            active = i;
        }
    }

  /* set the active item */
  gtk_combo_box_set_active (GTK_COMBO_BOX (combobox), active);
}



static void
display_setting_populate_refresh_rates (GladeXML *gxml,
                                        gint      selected_size)
{
    GtkTreeModel *model;
    GtkWidget    *combobox;
    GtkTreeIter   iter;
    gshort       *rates, current_rate;
    gshort        current_diff, diff = G_MAXSHORT;
    gint          active, n, nrates;
    gchar        *string;

    /* get the combo box store and clear it */
    combobox = glade_xml_get_widget (gxml, "screen-refresh-rate");
    model = gtk_combo_box_get_model (GTK_COMBO_BOX (combobox));
    gtk_list_store_clear (GTK_LIST_STORE (model));
  
    /* get refresh rates for this resolution */
    rates = XRRConfigRates (screen_config, selected_size, &nrates);
    current_rate = XRRConfigCurrentRate (screen_config);

    /* insert in store */
    for (active = n = 0; n < nrates; n++)
    {
        /* create string and insert into the store */
        string = g_strdup_printf ("%d %s", rates[n], _("Hz"));
        gtk_list_store_insert_with_values (GTK_LIST_STORE (model), &iter, n, COLUMN_COMBO_NAME, string, COLUMN_COMBO_VALUE, rates[n], -1);
        g_free (string);

        /* get the active item closest to the current diff */
        current_diff = ABS (rates[n] - current_rate);

        /* store active size */
        if (diff > current_diff)
          {
            active = n;
            diff = current_diff;
          }
    }
    
    /* select the active resolution */
    gtk_combo_box_set_active (GTK_COMBO_BOX (combobox), active);
}



static void
display_setting_populate_resolutions (GladeXML *gxml)
{
    GtkTreeModel  *model;
    GtkWidget     *combobox;
    GtkTreeIter    iter;
    SizeID         current_size;
    XRRScreenSize *sizes;
    gint           n, nsizes;
    Rotation       rotation;
    gchar         *string;

    /* get the combo box store and clear it */
    combobox = glade_xml_get_widget (gxml, "screen-resolution");
    model = gtk_combo_box_get_model (GTK_COMBO_BOX (combobox));
    gtk_list_store_clear (GTK_LIST_STORE (model));

    /* get current reolution and list or possible resolutions */
    sizes = XRRConfigSizes (screen_config, &nsizes);
    current_size = XRRConfigCurrentConfiguration (screen_config, &rotation);

    /* insert in store */
    for (n = 0; n < nsizes; n++)
    {
        /* insert resultion in to store */
        string = g_strdup_printf ("%d x %d", sizes[n].width, sizes[n].height);
        gtk_list_store_insert_with_values (GTK_LIST_STORE (model), &iter, n, 0, string, 1, n, -1);
        g_free (string);

        /* select active resolution */
        if (n == current_size)
          gtk_combo_box_set_active_iter (GTK_COMBO_BOX (combobox), &iter);
    }
}



static void
display_settings_populate_treeview (GladeXML *gxml)
{
    GtkWidget          *treeview;
    GtkListStore       *store;
    GdkDisplay         *display;
    gint                n, nscreens;
    Display            *xdisplay;
    XF86VidModeMonitor  monitor;
    gchar              *name;
    GtkTreePath        *path;
    GtkTreeSelection   *selection;
    
    /* create a new list store */
    store = gtk_list_store_new (N_MONITOR_COLUMNS, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INT);

    /* get the default display */
    display = gdk_display_get_default ();
    
    /* get the number of screens */
    nscreens = gdk_display_get_n_screens (display);

    /* get the x display */
    xdisplay = gdk_x11_display_get_xdisplay (display);
    
    /* walk the screens on this display */
    for (n = 0; n < nscreens; n++)
    {
        /* get the monitor information from x */
        if (XF86VidModeGetMonitor (xdisplay, n, &monitor) == True)
        {
            /* get a suitable name for in the treeview */
            if (IS_STRING (monitor.model) && IS_STRING (monitor.vendor)
                && strcasestr (monitor.model, monitor.vendor) == NULL)
                name = g_strdup_printf ("%s %s", monitor.vendor, monitor.model);
            else if (IS_STRING (monitor.model))
                name = g_strdup (monitor.model);
            else
                name = g_strdup_printf ("%s %d", _("Screen"), n + 1);
            
            /* insert in the store */
            gtk_list_store_insert_with_values (store, NULL, n,
                                               COLUMN_MONITOR_ID, n,
                                               COLUMN_MONITOR_ICON, "video-display",
                                               COLUMN_MONITOR_NAME, name, -1);
                                               
            /* cleanup */
            g_free (name);
        }
    }
    
    /* set the treeview model */
    treeview = glade_xml_get_widget (gxml, "devices-treeview");
    gtk_tree_view_set_model (GTK_TREE_VIEW (treeview), GTK_TREE_MODEL (store));
    g_object_unref (G_OBJECT (store));
    
    /* select first item */
    path = gtk_tree_path_new_first ();
    selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));
    gtk_tree_selection_select_path (selection, path);
    gtk_tree_path_free (path);
    
    /* treeview has initial focus */
    gtk_widget_grab_focus (treeview);
}



static void
display_settings_resolution_changed (GtkComboBox *combobox,
                                     GladeXML    *gxml)
{
    GtkTreeIter   iter;
    GtkTreeModel *model;
    gint          selected_size;
    
    /* get the active item */
    if (gtk_combo_box_get_active_iter (combobox, &iter))
    {
        /* get the combo box model and selected size */
        model = gtk_combo_box_get_model (combobox);
        gtk_tree_model_get (model, &iter, COLUMN_COMBO_VALUE, &selected_size, -1);
        
        /* update the refresh rate combo box */
        display_setting_populate_refresh_rates (gxml, selected_size);
    }
}



static void
display_settings_selection_changed (GtkTreeSelection *selection,
                                    GladeXML         *gxml)
{
    GtkTreeModel *model;
    GtkTreeIter   iter;
    gboolean      has_selection;
    gint          monitor_id;
    GdkDisplay   *display;
    GdkScreen    *screen;
    Display      *xdisplay;
    GdkWindow    *root_window;
    
    /* get the selection */
    has_selection = gtk_tree_selection_get_selected (selection, &model, &iter);
    if (G_LIKELY (has_selection))
    {
        /* get the monitor id */
        gtk_tree_model_get (model, &iter, COLUMN_MONITOR_ID, &monitor_id, -1);
        
        /* get the current display */
        display = gdk_display_get_default ();
        xdisplay = gdk_x11_display_get_xdisplay (display);
        
        /* get the screen selected in the treeview */
        screen = gdk_display_get_screen (display, monitor_id);
        
        /* get the root window of this screen */
        root_window = gdk_screen_get_root_window (screen);
        
        /* get the xrandr screen information */
        screen_config = XRRGetScreenInfo (xdisplay, gdk_x11_drawable_get_xid (GDK_DRAWABLE (root_window)));
        
        /* update dialog */
        display_setting_populate_resolutions (gxml);
        display_setting_populate_rotation (gxml);
    }
}



static void
display_settings_dialog_create_combobox_model (GtkComboBox *combobox)
{
    GtkCellRenderer *renderer;
    GtkListStore    *store;
    
    /* create and set the combobox model */
    store = gtk_list_store_new (N_COMBO_COLUMNS, G_TYPE_STRING, G_TYPE_INT);
    gtk_combo_box_set_model (combobox, GTK_TREE_MODEL (store));
    g_object_unref (G_OBJECT (store));
  
    /* setup renderer */
    renderer = gtk_cell_renderer_text_new ();
    gtk_cell_layout_clear (GTK_CELL_LAYOUT (combobox));
    gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combobox), renderer, TRUE);
    gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (combobox), renderer, "text", COLUMN_COMBO_NAME);
}



static GtkWidget *
display_settings_dialog_new_from_xml (GladeXML *gxml)
{
    GtkWidget        *treeview;
    GtkCellRenderer  *renderer;
    GtkTreeSelection *selection;
    GtkWidget        *combobox;
   
    /* get the treeview */
    treeview = glade_xml_get_widget (gxml, "devices-treeview");
#if GTK_CHECK_VERSION (2, 12, 0)
        gtk_tree_view_set_tooltip_column (GTK_TREE_VIEW (treeview), COLUMN_MONITOR_NAME);
#endif

    /* icon renderer */
    renderer = gtk_cell_renderer_pixbuf_new ();
    gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (treeview), 0, "", renderer, "icon-name", COLUMN_MONITOR_ICON, NULL);
    g_object_set (G_OBJECT (renderer), "stock-size", GTK_ICON_SIZE_DND, NULL);
   
    /* text renderer */
    renderer = gtk_cell_renderer_text_new ();
    gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (treeview), 1, "", renderer, "text", COLUMN_MONITOR_NAME, NULL);
    g_object_set (G_OBJECT (renderer), "ellipsize", PANGO_ELLIPSIZE_END, NULL);
    
    /* treeview selection */
    selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));
    gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);
    g_signal_connect (G_OBJECT (selection), "changed", G_CALLBACK (display_settings_selection_changed), gxml);
    
    /* setup the combo boxes */
    combobox = glade_xml_get_widget (gxml, "screen-resolution");
    display_settings_dialog_create_combobox_model (GTK_COMBO_BOX (combobox));
    g_signal_connect (G_OBJECT (combobox), "changed", G_CALLBACK (display_settings_resolution_changed), gxml);
    
    combobox = glade_xml_get_widget (gxml, "screen-refresh-rate");
    display_settings_dialog_create_combobox_model (GTK_COMBO_BOX (combobox));
    //g_signal_connect (G_OBJECT (combobox), "changed", G_CALLBACK (), gxml);
    
    combobox = glade_xml_get_widget (gxml, "screen-rotation");
    display_settings_dialog_create_combobox_model (GTK_COMBO_BOX (combobox));
    //g_signal_connect (G_OBJECT (combobox), "changed", G_CALLBACK (), gxml);
    
    /* populate the treeview */
    display_settings_populate_treeview (gxml);
   
    return glade_xml_get_widget (gxml, "display-dialog");
}



gint
main(gint argc, gchar **argv)
{
    GtkWidget *dialog;
    GladeXML  *gxml;
    GError    *error = NULL;

    /* setup translation domain */
    xfce_textdomain (GETTEXT_PACKAGE, LOCALEDIR, "UTF-8");

    /* initialize Gtk+ */
    if (!gtk_init_with_args (&argc, &argv, "", option_entries, GETTEXT_PACKAGE, &error))
    {
        if (G_LIKELY (error))
        {
            /* print error */
            g_print ("%s: %s.\n", G_LOG_DOMAIN, error->message);
            g_print (_("Type '%s --help' for usage."), G_LOG_DOMAIN);
            g_print ("\n");

            /* cleanup */
            g_error_free (error);
        }
        else
        {
            g_error ("Unable to open display.");
        }

        return EXIT_FAILURE;
    }

    /* print version information */
    if (G_UNLIKELY (opt_version))
    {
        g_print ("%s %s (Xfce %s)\n\n", G_LOG_DOMAIN, PACKAGE_VERSION, xfce_version_string ());
        g_print ("%s\n", "Copyright (c) 2004-2008");
        g_print ("\t%s\n\n", _("The Xfce development team. All rights reserved."));
        g_print (_("Please report bugs to <%s>."), PACKAGE_BUGREPORT);
        g_print ("\n");

        return EXIT_SUCCESS;
    }

    /* initialize xfconf */
    if (!xfconf_init (&error))
    {
        /* print error and exit */
        g_error ("Failed to connect to xfconf daemon: %s.", error->message);
        g_error_free (error);

        return EXIT_FAILURE;
    }

    /* open the xsettings channel */
    display_channel = xfconf_channel_new ("display");
    if (G_LIKELY (display_channel))
    {
        /* load the dialog glade xml */
        gxml = glade_xml_new_from_buffer (display_dialog_glade, display_dialog_glade_length, NULL, NULL);
        if (G_LIKELY (gxml))
        {
            /* build the dialog */
            dialog = display_settings_dialog_new_from_xml (gxml);

            /* run the dialog */
            gtk_dialog_run (GTK_DIALOG (dialog));

            /* release the glade xml */
            g_object_unref (G_OBJECT (gxml));

            /* destroy the dialog */
            gtk_widget_destroy (dialog);
        }

        /* release the channel */
        g_object_unref (G_OBJECT (display_channel));
    }

    /* shutdown xfconf */
    xfconf_shutdown ();

    return EXIT_SUCCESS;
}
