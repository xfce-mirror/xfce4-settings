/* $Id$ */
/*
 *  Copyright (c) 2008 Stephan Arts <stephan@xfce.org>
 *                     Jannis Pohlmann <jannis@xfce.org>
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

#include <libxfce4ui/libxfce4ui.h>
#include <libxfce4util/libxfce4util.h>
#include <xfconf/xfconf.h>

#include "accessibility-dialog_ui.h"



static GdkNativeWindow opt_socket_id = 0;
static gboolean opt_version = FALSE;
static GOptionEntry entries[] =
{
    { "socket-id", 's', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_INT, &opt_socket_id, N_("Settings manager socket"), N_("SOCKET ID") },
    { "version", 'v', 0, G_OPTION_ARG_NONE, &opt_version, N_("Version information"), NULL },
    { NULL }
};



/* global xfconf channel */
static XfconfChannel *accessibility_channel = NULL;



static void
accessibility_settings_sensitivity (GtkToggleButton *button,
                                    GtkWidget       *box)
{
    gtk_widget_set_sensitive (GTK_WIDGET (box),
        gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button)));
}



static void
accessibility_settings_dialog_configure_widgets (GtkBuilder *builder)
{
    GObject *box, *object;

    /* Sticky keys */
    object = gtk_builder_get_object (builder, "sticky-keys-enabled");
    box = gtk_builder_get_object (builder, "sticky-keys-box");
    g_signal_connect (object, "toggled", G_CALLBACK (accessibility_settings_sensitivity), box);
    xfconf_g_property_bind (accessibility_channel, "/StickyKeys", G_TYPE_BOOLEAN, object, "active");

    object = gtk_builder_get_object (builder, "sticky-keys-latch-to-lock");
    xfconf_g_property_bind (accessibility_channel, "/StickyKeys/LatchToLock", G_TYPE_BOOLEAN, object, "active");

    object = gtk_builder_get_object (builder, "sticky-keys-two-keys-disable");
    xfconf_g_property_bind (accessibility_channel, "/StickyKeys/TwoKeysDisable", G_TYPE_BOOLEAN, object, "active");

    /* Slow keys */
    object = gtk_builder_get_object (builder, "slow-keys-enabled");
    box = gtk_builder_get_object (builder, "slow-keys-box");
    g_signal_connect (object, "toggled", G_CALLBACK (accessibility_settings_sensitivity), box);
    xfconf_g_property_bind (accessibility_channel, "/SlowKeys", G_TYPE_BOOLEAN, object, "active");

    object = gtk_builder_get_object (builder, "slow-keys-delay");
    xfconf_g_property_bind (accessibility_channel, "/SlowKeys/Delay", G_TYPE_INT, object, "value");

    /* Bounce keys */
    object = gtk_builder_get_object (builder, "bounce-keys-enabled");
    box = gtk_builder_get_object (builder, "bounce-keys-box");
    g_signal_connect (object, "toggled", G_CALLBACK (accessibility_settings_sensitivity), box);
    xfconf_g_property_bind (accessibility_channel, "/BounceKeys", G_TYPE_BOOLEAN, object, "active");

    object = gtk_builder_get_object (builder, "bounce-keys-delay");
    xfconf_g_property_bind (accessibility_channel, "/BounceKeys/Delay", G_TYPE_INT, object, "value");

    /* Mouse keys */
    object = gtk_builder_get_object (builder, "mouse-emulation-enabled");
    box = gtk_builder_get_object (builder, "mouse-emulation-box");
    g_signal_connect (object, "toggled", G_CALLBACK (accessibility_settings_sensitivity), box);
    xfconf_g_property_bind (accessibility_channel, "/MouseKeys", G_TYPE_BOOLEAN, object, "active");

    object = gtk_builder_get_object (builder, "mouse-emulation-delay");
    xfconf_g_property_bind (accessibility_channel, "/MouseKeys/Delay", G_TYPE_INT, object, "value");

    object = gtk_builder_get_object (builder, "mouse-emulation-interval");
    xfconf_g_property_bind (accessibility_channel, "/MouseKeys/Interval", G_TYPE_INT, object, "value");

    object = gtk_builder_get_object (builder, "mouse-emulation-time-to-max");
    xfconf_g_property_bind (accessibility_channel, "/MouseKeys/TimeToMax", G_TYPE_INT, object, "value");

    object = gtk_builder_get_object (builder, "mouse-emulation-max-speed");
    xfconf_g_property_bind (accessibility_channel, "/MouseKeys/MaxSpeed", G_TYPE_INT, object, "value");

    object = gtk_builder_get_object (builder, "mouse-emulation-curve");
    xfconf_g_property_bind (accessibility_channel, "/MouseKeys/Curve", G_TYPE_INT, object, "value");
}



gint
main (gint argc, gchar **argv)
{
    GObject    *dialog, *plug_child;
    GtkWidget  *plug;
    GtkBuilder *builder;
    GError     *error = NULL;

    /* setup translation domain */
    xfce_textdomain (GETTEXT_PACKAGE, LOCALEDIR, "UTF-8");

    /* initialize Gtk+ */
    if(!gtk_init_with_args (&argc, &argv, "", entries, PACKAGE, &error))
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

    /* check if we should print version information */
    if (G_UNLIKELY (opt_version))
    {
        g_print ("%s %s (Xfce %s)\n\n", G_LOG_DOMAIN, PACKAGE_VERSION, xfce_version_string ());
        g_print ("%s\n", "Copyright (c) 2008-2009");
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
    accessibility_channel = xfconf_channel_new ("accessibility");

    /* hook to make sure the libxfce4ui library is linked */
    if (xfce_titled_dialog_get_type () == 0)
        return EXIT_FAILURE;

    /* load the Gtk user-interface file */
    builder = gtk_builder_new ();
    if (gtk_builder_add_from_string (builder, accessibility_dialog_ui,
                                     accessibility_dialog_ui_length, &error) != 0)
    {
        /* Configure widgets */
        accessibility_settings_dialog_configure_widgets (builder);

        if (G_UNLIKELY (opt_socket_id == 0))
        {
            /* Get the dialog widget */
            dialog = gtk_builder_get_object (builder, "dialog");

            /* run the dialog */
            gtk_dialog_run (GTK_DIALOG (dialog));

            /* destroy the dialog */
            gtk_widget_destroy (GTK_WIDGET (dialog));
        }
        else
        {
            /* Create plug widget */
            plug = gtk_plug_new (opt_socket_id);
            g_signal_connect (plug, "delete-event", G_CALLBACK (gtk_main_quit), NULL);
            gtk_widget_show (plug);

            /* Stop startup notification */
            gdk_notify_startup_complete ();

            /* Get plug child widget */
            plug_child = gtk_builder_get_object (builder, "plug-child");
            gtk_widget_reparent (GTK_WIDGET (plug_child), plug);
            gtk_widget_show (GTK_WIDGET (plug_child));

            /* Enter main loop */
            gtk_main ();
        }
    }
    else
    {
        g_error ("Failed to load the UI file: %s.", error->message);
        g_error_free (error);
    }

    /* Release Builder */
    g_object_unref (G_OBJECT (builder));

    /* release the channel */
    g_object_unref (G_OBJECT (accessibility_channel));

    /* shutdown xfconf */
    xfconf_shutdown();

    return EXIT_SUCCESS;
}
