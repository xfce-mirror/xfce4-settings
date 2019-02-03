/*
 *  Copyright (c) 2018 Simon Steinbeiß <simon@xfce.org>
 *                     Florian Schüller <florian.schueller@gmail.com>
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
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#include <colord.h>
#include <glib.h>
#include <gtk/gtk.h>
#include <gtk/gtkx.h>

#include <gdk/gdkx.h>

#include <libxfce4ui/libxfce4ui.h>
#include <libxfce4util/libxfce4util.h>
#include <xfconf/xfconf.h>

#include "color-dialog_ui.h"



static gint opt_socket_id = 0;
static gboolean opt_version = FALSE;
static GOptionEntry entries[] =
{
    { "socket-id", 's', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_INT, &opt_socket_id, N_("Settings manager socket"), N_("SOCKET ID") },
    { "version", 'v', 0, G_OPTION_ARG_NONE, &opt_version, N_("Version information"), NULL },
    { NULL }
};



/* global xfconf channel */
static XfconfChannel *color_channel = NULL;

typedef
struct _ColorSettings
{
    CdClient      *client;
    CdDevice      *current_device;
    GPtrArray     *devices;
    GCancellable  *cancellable;
    GDBusProxy    *proxy;
} ColorSettings;

static ColorSettings *color_settings;

static void
color_settings_device_selected_cb (GtkTreeView       *tree_view,
                                   GtkTreePath       *path,
                                   GtkTreeViewColumn *column,
                                   gpointer           user_data)
{

}



static void
color_settings_profile_add_cb (GtkButton *button, gpointer user_data)
{

}



static void
color_settings_dialog_configure_widgets (GtkBuilder *builder)
{
    GObject *devices, *profiles, *profile_add;

    /* Sticky keys */
    devices = gtk_builder_get_object (builder, "colord-devices");
    profiles = gtk_builder_get_object (builder, "colord-profiles");
    g_signal_connect (devices, "row-activated", G_CALLBACK (color_settings_device_selected_cb), profiles);

    profile_add = gtk_builder_get_object (builder, "profile-add");
    g_signal_connect (profile_add, "clicked", G_CALLBACK (color_settings_profile_add_cb), NULL);
}



static void
color_settings_dialog_response (GtkWidget *dialog,
                                        gint       response_id)
{
    if (response_id == GTK_RESPONSE_HELP)
        xfce_dialog_show_help_with_version (GTK_WINDOW (dialog), "xfce4-settings", "color",
                                            NULL, XFCE4_SETTINGS_VERSION_SHORT);
    else
        gtk_main_quit ();
}



static void
color_settings_get_devices_cb (GObject *object,
                               GAsyncResult *res,
                               gpointer user_data)
{
  //ColorSettings *settings = (ColorSettings *) user_data;
  CdClient *client = CD_CLIENT (object);
  //CdDevice *device;
  g_autoptr(GError) error = NULL;
  g_autoptr(GPtrArray) devices = NULL;
  guint i;

  /* get devices and add them */
  devices = cd_client_get_devices_finish (client, res, &error);
  if (devices == NULL)
    {
      g_warning ("failed to add connected devices: %s",
                 error->message);
      return;
    }
  for (i = 0; i < devices->len; i++)
    {
      //device = g_ptr_array_index (devices, i);
      g_warning ("device: %d", i);
      //gcm_prefs_add_device (prefs, device);
    }

  /* ensure we show the 'No devices detected' entry if empty */
  //gcm_prefs_update_device_list_extra_entry (prefs);
}



static void
color_settings_connect_cb (GObject *object,
                           GAsyncResult *res,
                           gpointer user_data)
{
    gboolean ret;
    g_autoptr(GError) error = NULL;

    ret = cd_client_connect_finish (CD_CLIENT (object),
                                    res,
                                    &error);
    if (!ret)
      {
        if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
          g_warning ("failed to connect to colord: %s", error->message);
        return;
      }

    /* Only cast the parameters after making sure it didn't fail. At this point,
     * the user can potentially already have changed to another panel, effectively
     * making user_data invalid. */
    //settings = CC_COLOR_PANEL (user_data);

    /* get devices */
    cd_client_get_devices (color_settings->client,
                           color_settings->cancellable,
                           color_settings_get_devices_cb,
                           color_settings);
}



static void
color_settings_dialog_init (void)
{
    color_settings = g_new0 (ColorSettings, 1);
    color_settings->cancellable = g_cancellable_new ();
    color_settings->devices = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);

    /* use a device client array */
    color_settings->client = cd_client_new ();
/*    g_signal_connect_object (settings->client, "device-added",
                             G_CALLBACK (color_settings_device_added_cb), settings, 0);
    g_signal_connect_object (settings->client, "device-removed",
                             G_CALLBACK (color_settings_device_removed_cb), settings, 0);
*/
    cd_client_connect (color_settings->client,
                       color_settings->cancellable,
                       color_settings_connect_cb,
                       color_settings);
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
        g_print ("%s\n", "Copyright (c) 2008-2018");
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

    /* open the channels */
    color_channel = xfconf_channel_new ("color");

    /* hook to make sure the libxfce4ui library is linked */
    if (xfce_titled_dialog_get_type () == 0)
        return EXIT_FAILURE;

    /* load the Gtk user-interface file */
    builder = gtk_builder_new ();
    if (gtk_builder_add_from_string (builder, color_dialog_ui,
                                     color_dialog_ui_length, &error) != 0)
    {
        /* Configure widgets */
        color_settings_dialog_init ();
        color_settings_dialog_configure_widgets (builder);

        if (G_UNLIKELY (opt_socket_id == 0))
        {
            /* Get the dialog widget */
            dialog = gtk_builder_get_object (builder, "dialog");

            g_signal_connect (dialog, "response",
                G_CALLBACK (color_settings_dialog_response), NULL);
            gtk_window_present (GTK_WINDOW (dialog));

            /* To prevent the settings dialog to be saved in the session */
            gdk_x11_set_sm_client_id ("FAKE ID");

            gtk_main ();
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
            xfce_widget_reparent (GTK_WIDGET (plug_child), plug);
            gtk_widget_show (GTK_WIDGET (plug_child));

            /* To prevent the settings dialog to be saved in the session */
            gdk_x11_set_sm_client_id ("FAKE ID");

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

    /* release the channels */
    g_object_unref (G_OBJECT (color_channel));

    /* shutdown xfconf */
    xfconf_shutdown();

    return EXIT_SUCCESS;
}
