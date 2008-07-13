/*
 *  Copyright (c) 2008 Stephan Arts <stephan@xfce.org>
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

#include <glib.h>
#include <gtk/gtk.h>
#include <glade/glade.h>

#include <libxfcegui4/libxfcegui4.h>
#include <libxfce4util/libxfce4util.h>
#include <xfconf/xfconf.h>

#include "accessibility-dialog_glade.h"


gboolean opt_version = FALSE;
static GOptionEntry entries[] =
{
    { "version", 'v', 0, G_OPTION_ARG_NONE, &opt_version, N_("Version information"), NULL },
    { NULL }
};

static XfconfChannel *accessx_channel;

void
cb_xkb_accessx_mouse_toggled (GtkToggleButton *button, gpointer user_data)
{
    GladeXML *gxml = GLADE_XML(user_data);
    GtkWidget *box = glade_xml_get_widget (gxml, "xkb_accessx_mouse_box");
    gtk_widget_set_sensitive (box, gtk_toggle_button_get_active (button));
}

void
cb_xkb_accessx_sticky_toggled (GtkToggleButton *button, gpointer user_data)
{
    GladeXML *gxml = GLADE_XML(user_data);
    GtkWidget *box = glade_xml_get_widget (gxml, "xkb_accessx_sticky_box");
    gtk_widget_set_sensitive (box, gtk_toggle_button_get_active (button));
}

void
cb_xkb_accessx_slow_toggled (GtkToggleButton *button, gpointer user_data)
{
    GladeXML *gxml = GLADE_XML(user_data);
    GtkWidget *box = glade_xml_get_widget (gxml, "xkb_accessx_slow_box");
    gtk_widget_set_sensitive (box, gtk_toggle_button_get_active (button));
}

void
cb_xkb_accessx_bounce_toggled (GtkToggleButton *button, gpointer user_data)
{
    GladeXML *gxml = GLADE_XML(user_data);
    GtkWidget *box = glade_xml_get_widget (gxml, "xkb_accessx_bounce_box");
    gtk_widget_set_sensitive (box, gtk_toggle_button_get_active (button));
}

GtkWidget *
accessibility_settings_dialog_new_from_xml (GladeXML *gxml)
{
    GtkWidget *xkb_accessx_mouse_check = glade_xml_get_widget (gxml, "xkb_accessx_mouse_check");
    GtkWidget *xkb_accessx_mouse_speed_scale =(GtkWidget *)gtk_range_get_adjustment(GTK_RANGE(glade_xml_get_widget (gxml, "xkb_accessx_mouse_speed_scale")));
    GtkWidget *xkb_accessx_mouse_delay_scale =(GtkWidget *)gtk_range_get_adjustment(GTK_RANGE(glade_xml_get_widget (gxml, "xkb_accessx_mouse_delay_scale")));
    GtkWidget *xkb_accessx_mouse_acceldelay_scale =(GtkWidget *)gtk_range_get_adjustment(GTK_RANGE(glade_xml_get_widget (gxml, "xkb_accessx_mouse_acceldelay_scale")));
    GtkWidget *xkb_accessx_mouse_interval_scale =(GtkWidget *)gtk_range_get_adjustment(GTK_RANGE(glade_xml_get_widget (gxml, "xkb_accessx_mouse_interval_scale")));

    GtkWidget *xkb_accessx_sticky_check = glade_xml_get_widget (gxml, "xkb_accessx_sticky_check");
    GtkWidget *xkb_accessx_sticky_lock_mode = glade_xml_get_widget (gxml, "xkb_accessx_sticky_lock_mode");
    GtkWidget *xkb_accessx_sticky_two_keys_disable_check = glade_xml_get_widget (gxml, "xkb_accessx_sticky_two_keys_disable_check");
    GtkWidget *xkb_accessx_slow_check = glade_xml_get_widget (gxml, "xkb_accessx_slow_check");
    GtkWidget *xkb_accessx_slow_delay_scale = (GtkWidget *)gtk_range_get_adjustment(GTK_RANGE(glade_xml_get_widget (gxml, "xkb_accessx_slow_delay_scale")));
    GtkWidget *xkb_accessx_bounce_check = glade_xml_get_widget (gxml, "xkb_accessx_bounce_check");
    GtkWidget *xkb_accessx_debounce_delay_scale =(GtkWidget *)gtk_range_get_adjustment(GTK_RANGE(glade_xml_get_widget (gxml, "xkb_accessx_debounce_delay_scale")));

    g_signal_connect (G_OBJECT(xkb_accessx_mouse_check), "toggled", (GCallback)cb_xkb_accessx_mouse_toggled, gxml);
    g_signal_connect (G_OBJECT(xkb_accessx_sticky_check), "toggled", (GCallback)cb_xkb_accessx_sticky_toggled, gxml);
    g_signal_connect (G_OBJECT(xkb_accessx_slow_check), "toggled", (GCallback)cb_xkb_accessx_slow_toggled, gxml);
    g_signal_connect (G_OBJECT(xkb_accessx_bounce_check), "toggled", (GCallback)cb_xkb_accessx_bounce_toggled, gxml);


    /* Bind easy properties */
    /* Mouse settings */
    xfconf_g_property_bind (accessx_channel, 
                            "/AccessX/MouseKeys",
                            G_TYPE_BOOLEAN,
                            (GObject *)xkb_accessx_mouse_check, "active");
    xfconf_g_property_bind (accessx_channel, 
                            "/AccessX/MouseKeys/Speed",
                            G_TYPE_INT,
                            (GObject *)xkb_accessx_mouse_speed_scale, "value");
    xfconf_g_property_bind (accessx_channel, 
                            "/AccessX/MouseKeys/Delay",
                            G_TYPE_INT,
                            (GObject *)xkb_accessx_mouse_delay_scale, "value");
    xfconf_g_property_bind (accessx_channel, 
                            "/AccessX/MouseKeys/Interval",
                            G_TYPE_INT,
                            (GObject *)xkb_accessx_mouse_interval_scale, "value");
    xfconf_g_property_bind (accessx_channel, 
                            "/AccessX/MouseKeys/TimeToMax",
                            G_TYPE_INT,
                            (GObject *)xkb_accessx_mouse_acceldelay_scale, "value");

    /* Keyboard settings */
    xfconf_g_property_bind (accessx_channel, 
                            "/AccessX/StickyKeys",
                            G_TYPE_BOOLEAN,
                            (GObject *)xkb_accessx_sticky_check, "active");
    xfconf_g_property_bind (accessx_channel, 
                            "/AccessX/StickyKeys/LatchToLock",
                            G_TYPE_BOOLEAN,
                            (GObject *)xkb_accessx_sticky_lock_mode, "active");
    xfconf_g_property_bind (accessx_channel, 
                            "/AccessX/StickyKeys/TwoKeysDisable",
                            G_TYPE_BOOLEAN,
                            (GObject *)xkb_accessx_sticky_two_keys_disable_check, "active");
    xfconf_g_property_bind (accessx_channel, 
                            "/AccessX/BounceKeys",
                            G_TYPE_BOOLEAN,
                            (GObject *)xkb_accessx_bounce_check, "active");
    xfconf_g_property_bind (accessx_channel, 
                            "/AccessX/BounceKeys/Delay",
                            G_TYPE_INT,
                            (GObject *)xkb_accessx_debounce_delay_scale, "value");
    xfconf_g_property_bind (accessx_channel, 
                            "/AccessX/SlowKeys",
                            G_TYPE_BOOLEAN,
                            (GObject *)xkb_accessx_slow_check, "active");
    xfconf_g_property_bind (accessx_channel, 
                            "/AccessX/SlowKeys/Delay",
                            G_TYPE_INT,
                            (GObject *)xkb_accessx_slow_delay_scale, "value");

    return glade_xml_get_widget (gxml, "accessibility-settings-dialog");
}

int
main(int argc, char **argv)
{
    GladeXML  *gxml;
    GError    *error = NULL;
    GtkWidget *dialog;

    /* setup translation domain */
    xfce_textdomain (GETTEXT_PACKAGE, LOCALEDIR, "UTF-8");

    /* initialize Gtk+ */
    if(!gtk_init_with_args(&argc, &argv, "", entries, PACKAGE, &error))
    {
        if (G_LIKELY (error))
        {
            /* print error */
            g_print ("xfce4-accessibility-settings: %s.\n", error->message);
            g_print (_("Type '%s --help' for usage."), "xfce4-accessibility-settings");
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

    /* check if we should print version information */
    if (G_UNLIKELY (opt_version))
    {
        g_print ("xfce4-settings-helper %s\n\n", PACKAGE_VERSION);
        g_print ("%s\n", "Copyright (c) 2008");
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
    
    /* open the channel */
    accessx_channel = xfconf_channel_new("accessx");

    /* load the glade interface */
    gxml = glade_xml_new_from_buffer (accessibility_dialog_glade,
                                      accessibility_dialog_glade_length,
                                      NULL, NULL);

    /* get the dialog */
    dialog = accessibility_settings_dialog_new_from_xml (gxml);

    /* run the dialog */
    gtk_dialog_run (GTK_DIALOG (dialog));
    
    /* destroy the dialog */
    gtk_widget_destroy (dialog);
    
    /* release the channel */
    g_object_unref (G_OBJECT (accessx_channel));

    /* shutdown xfconf */
    xfconf_shutdown();

    return EXIT_SUCCESS;
}
