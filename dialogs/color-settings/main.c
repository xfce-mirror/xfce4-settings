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

#include "color-device.h"
#include "color-profile.h"
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
    GObject       *grid;
    GObject       *label_no_devices;
    GObject       *box_devices;
    GObject       *frame_devices;
    GtkListBox    *list_box;
    gchar         *list_box_filter;
    guint          list_box_selected_id;
    guint          list_box_activated_id;
    GtkSizeGroup  *list_box_size;
    GObject       *label_no_profiles;
    GObject       *box_profiles;
    GObject       *frame_profiles;
    GtkListBox    *profiles_list_box;
    gchar         *profiles_list_box_filter;
    guint          profiles_list_box_selected_id;
    guint          profiles_list_box_activated_id;
    GtkSizeGroup  *profiles_list_box_size;
} ColorSettings;



static void
color_settings_make_profile_default_cb (GObject *object,
                                        GAsyncResult *res,
                                        ColorSettings *settings)
{
  CdDevice *device = CD_DEVICE (object);
  gboolean ret = FALSE;
  g_autoptr(GError) error = NULL;

  ret = cd_device_make_profile_default_finish (device,
                                               res,
                                               &error);
  if (!ret)
    {
      g_warning ("failed to set default profile on %s: %s",
                 cd_device_get_id (device),
                 error->message);
    }
}



static void
color_settings_device_profile_enable_cb (GtkWidget *widget, ColorSettings *settings)
{
  CdProfile *profile;
  GtkListBoxRow *row;

  /* get the selected profile */
  row = gtk_list_box_get_selected_row (settings->profiles_list_box);
  if (row == NULL)
    return;
  profile = color_profile_get_profile (SETTINGS_COLOR_PROFILE (row));
  if (profile == NULL)
    {
        g_warning ("failed to get the active profile");
        return;
    }

  /* just set it default */
  g_debug ("setting %s default on %s",
           cd_profile_get_id (profile),
           cd_device_get_id (settings->current_device));
  cd_device_make_profile_default (settings->current_device,
                                  profile,
                                  settings->cancellable,
                                  (GAsyncReadyCallback) color_settings_make_profile_default_cb,
                                  settings);
}

static void
color_settings_add_device_profile (ColorSettings *settings,
                              CdDevice *device,
                              CdProfile *profile,
                              gboolean is_default)
{
  gboolean ret;
  g_autoptr(GError) error = NULL;
  GtkWidget *widget;

  /* get properties */
  ret = cd_profile_connect_sync (profile,
                                 settings->cancellable,
                                 &error);
  if (!ret)
    {
      g_warning ("failed to get profile: %s", error->message);
      return;
    }

  /* ignore profiles from other user accounts */
  if (!cd_profile_has_access (profile))
    {
      /* only print the filename if it exists */
      if (cd_profile_get_filename (profile) != NULL)
        {
          g_warning ("%s is not usable by this user",
                     cd_profile_get_filename (profile));
        }
      else
        {
          g_warning ("%s is not usable by this user",
                     cd_profile_get_id (profile));
        }
      return;
    }

  /* add to listbox */
  widget = color_profile_new (device, profile, is_default);
  gtk_widget_show (widget);
  gtk_container_add (GTK_CONTAINER (settings->profiles_list_box), widget);
  gtk_size_group_add_widget (settings->profiles_list_box_size, widget);
}



static void
color_settings_add_device_profiles (ColorSettings *settings, CdDevice *device)
{
  CdProfile *profile_tmp;
  g_autoptr(GPtrArray) profiles = NULL;
  guint i;

  /* add profiles */
  profiles = cd_device_get_profiles (device);
  if (profiles == NULL)
    return;
  for (i = 0; i < profiles->len; i++)
    {
      profile_tmp = g_ptr_array_index (profiles, i);
      color_settings_add_device_profile (settings, device, profile_tmp, i == 0);
    }

  gtk_widget_show (GTK_WIDGET (settings->profiles_list_box));
}



/* find the profile in the array -- for flicker-free changes */
static gboolean
color_settings_find_profile_by_object_path (GPtrArray *profiles,
                                       const gchar *object_path)
{
  CdProfile *profile_tmp;
  guint i;

  for (i = 0; i < profiles->len; i++)
    {
      profile_tmp = g_ptr_array_index (profiles, i);
      if (g_strcmp0 (cd_profile_get_object_path (profile_tmp), object_path) == 0)
        return TRUE;
    }
  return FALSE;
}



/* find the profile in the list view -- for flicker-free changes */
static gboolean
color_settings_find_widget_by_object_path (GList *list,
                                      const gchar *object_path_device,
                                      const gchar *object_path_profile)
{
  GList *l;
  CdDevice *device_tmp;
  CdProfile *profile_tmp;

  for (l = list; l != NULL; l = l->next)
    {
      if (!SETTINGS_IS_COLOR_PROFILE (l->data))
        continue;

      /* correct device ? */
      device_tmp = color_profile_get_device (SETTINGS_COLOR_PROFILE (l->data));
      if (g_strcmp0 (object_path_device,
                     cd_device_get_object_path (device_tmp)) != 0)
        {
          continue;
        }

      /* this profile */
      profile_tmp = color_profile_get_profile (SETTINGS_COLOR_PROFILE (l->data));
      if (g_strcmp0 (object_path_profile,
                     cd_profile_get_object_path (profile_tmp)) == 0)
        {
          return TRUE;
        }
    }
  return FALSE;
}



static void
color_settings_update_device_list_extra_entry (ColorSettings *settings)
{
  g_autoptr(GList) device_widgets = NULL;
  guint number_of_devices;

  /* any devices to show? */
  device_widgets = gtk_container_get_children (GTK_CONTAINER (settings->list_box));
  number_of_devices = g_list_length (device_widgets);
  gtk_widget_set_visible (GTK_WIDGET (settings->label_no_devices), number_of_devices == 0);
  gtk_widget_set_visible (GTK_WIDGET (settings->box_devices), number_of_devices > 0);
}



static void
color_settings_update_profile_list_extra_entry (ColorSettings *settings)
{
  g_autoptr(GList) profile_widgets = NULL;
  guint number_of_profiles;

  /* any profiles to show? */
  profile_widgets = gtk_container_get_children (GTK_CONTAINER (settings->profiles_list_box));
  number_of_profiles = g_list_length (profile_widgets);
  gtk_widget_set_visible (GTK_WIDGET (settings->label_no_profiles), number_of_profiles == 0);
  gtk_widget_set_visible (GTK_WIDGET (settings->box_profiles), number_of_profiles > 0);
}



static void
listbox_remove_all (GtkWidget *widget, gpointer user_data)
{
    GtkWidget *container = user_data;
    gtk_container_remove (GTK_CONTAINER (container), widget);
}



static void
color_settings_list_box_row_activated_cb (GtkListBox *list_box,
                                          GtkListBoxRow *row,
                                          ColorSettings *settings)
{
    GtkCallback func = listbox_remove_all;

    gtk_container_foreach (GTK_CONTAINER (settings->profiles_list_box), func, settings->profiles_list_box);
    g_object_get (row, "device", &settings->current_device, NULL);
    if (cd_device_get_enabled (settings->current_device))
    {
        color_settings_add_device_profiles (settings, settings->current_device);
        color_settings_update_profile_list_extra_entry (settings);
        gtk_widget_queue_draw (GTK_WIDGET (settings->grid));
    }
    else
        color_settings_update_profile_list_extra_entry (settings);
}




static void
color_settings_device_enabled_changed_cb (ColorDevice *widget,
                                          gboolean is_enabled,
                                          ColorSettings *settings)
{
    gtk_list_box_select_row (settings->list_box, GTK_LIST_BOX_ROW (widget));
    gtk_widget_set_visible (GTK_WIDGET (settings->label_no_profiles), !is_enabled);
    gtk_widget_set_visible (GTK_WIDGET (settings->box_profiles), is_enabled);
}



static void
color_settings_profiles_list_box_row_activated_cb (GtkListBox *list_box,
                                                   GtkListBoxRow *row,
                                                   ColorSettings *settings)
{
    if (SETTINGS_IS_COLOR_PROFILE (row))
    {
      color_settings_device_profile_enable_cb (NULL, settings);
    }
}



static void
color_settings_profile_add_cb (GtkButton *button, gpointer user_data)
{

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
color_settings_device_changed_cb (CdDevice *device, ColorSettings *settings)
{
  CdDevice *device_tmp;
  CdProfile *profile_tmp;
  gboolean ret;
  GList *l;
  g_autoptr(GList) list = NULL;
  GPtrArray *profiles;
  guint i;

  /* remove anything in the list view that's not in Device.Profiles */
  profiles = cd_device_get_profiles (device);
  list = gtk_container_get_children (GTK_CONTAINER (settings->profiles_list_box));
  for (l = list; l != NULL; l = l->next)
    {
      if (!SETTINGS_IS_COLOR_PROFILE (l->data))
        continue;

      /* correct device ? */
      device_tmp = color_profile_get_device (SETTINGS_COLOR_PROFILE (l->data));
      if (g_strcmp0 (cd_device_get_id (device),
                     cd_device_get_id (device_tmp)) != 0)
        continue;

      /* if profile is not in Device.Profiles then remove */
      profile_tmp = color_profile_get_profile (SETTINGS_COLOR_PROFILE (l->data));
      ret = color_settings_find_profile_by_object_path (profiles,
                                                   cd_profile_get_object_path (profile_tmp));
      if (!ret) {
        gtk_widget_destroy (GTK_WIDGET (l->data));
        /* Don't look at the destroyed widget below */
        l->data = NULL;
      }
    }

  /* add anything in Device.Profiles that's not in the list view */
  for (i = 0; i < profiles->len; i++)
    {
      profile_tmp = g_ptr_array_index (profiles, i);
      ret = color_settings_find_widget_by_object_path (list,
                                                  cd_device_get_object_path (device),
                                                  cd_profile_get_object_path (profile_tmp));
      if (!ret)
        color_settings_add_device_profile (settings, device, profile_tmp, i == 0);
    }

  color_settings_update_profile_list_extra_entry (settings);
  /* resort */
  //gtk_list_box_invalidate_sort (prefs->list_box);
}



static void
color_settings_add_device (ColorSettings *settings, CdDevice *device)
{
  gboolean ret;
  g_autoptr(GError) error = NULL;
  GtkWidget *widget;

  /* get device properties */
  ret = cd_device_connect_sync (device, settings->cancellable, &error);
  if (!ret)
    {
      g_warning ("failed to connect to the device: %s", error->message);
      return;
    }

  /* add device */
  widget = color_device_new (device);
  g_signal_connect (G_OBJECT (widget), "enabled-changed",
                    G_CALLBACK (color_settings_device_enabled_changed_cb), settings);
  gtk_widget_show (widget);
  gtk_container_add (GTK_CONTAINER (settings->list_box), widget);
  gtk_size_group_add_widget (settings->list_box_size, widget);

  /* add profiles */
  //color_settings_add_device_profiles (settings, device);

  /* watch for changes */
  g_ptr_array_add (settings->devices, g_object_ref (device));
  g_signal_connect (device, "changed",
                    G_CALLBACK (color_settings_device_changed_cb), settings);
  gtk_list_box_invalidate_sort (settings->list_box);
}



static void
color_settings_remove_device (ColorSettings *settings, CdDevice *device)
{
  CdDevice *device_tmp;
  GList *l;
  g_autoptr(GList) list = NULL;

  list = gtk_container_get_children (GTK_CONTAINER (settings->list_box));
  for (l = list; l != NULL; l = l->next)
    {
      device_tmp = color_device_get_device (CC_COLOR_DEVICE (l->data));

      if (g_strcmp0 (cd_device_get_object_path (device),
                     cd_device_get_object_path (device_tmp)) == 0)
        {
          gtk_widget_destroy (GTK_WIDGET (l->data));
        }
    }
  g_signal_handlers_disconnect_by_func (device,
                                        G_CALLBACK (color_settings_device_changed_cb),
                                        settings);
  g_ptr_array_remove (settings->devices, device);
  color_settings_update_profile_list_extra_entry (settings);
}



static void
list_box_update_header_func (GtkListBoxRow *row,
                                GtkListBoxRow *before,
                                gpointer user_data)
{
  GtkWidget *current;

  if (before == NULL)
    {
      gtk_list_box_row_set_header (row, NULL);
      return;
    }

  current = gtk_list_box_row_get_header (row);
  if (current == NULL)
    {
      current = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
      gtk_widget_show (current);
      gtk_list_box_row_set_header (row, current);
    }
}



static void
color_settings_device_added_cb (CdClient *client,
                                CdDevice *device,
                                ColorSettings *settings)
{
  /* add the device */
  color_settings_add_device (settings, device);

  /* ensure we're not showing the 'No devices detected' entry */
  color_settings_update_device_list_extra_entry (settings);
}



static void
color_settings_device_removed_cb (CdClient *client,
                                  CdDevice *device,
                                  ColorSettings *settings)
{
  /* remove from the UI */
  color_settings_remove_device (settings, device);

  /* ensure we showing the 'No devices detected' entry if required */
  color_settings_update_device_list_extra_entry (settings);
}



static void
color_settings_get_devices_cb (GObject *object,
                               GAsyncResult *res,
                               gpointer user_data)
{
  ColorSettings *settings = (ColorSettings *) user_data;
  CdClient *client = CD_CLIENT (object);
  CdDevice *device;
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
      device = g_ptr_array_index (devices, i);
      color_settings_add_device (settings, device);
    }
  /* ensure we showing the 'No devices detected' entry if required */
  color_settings_update_device_list_extra_entry (settings);
  color_settings_update_profile_list_extra_entry (settings);
}



static void
color_settings_connect_cb (GObject *object,
                           GAsyncResult *res,
                           gpointer user_data)
{
    ColorSettings *settings = (ColorSettings *) user_data;
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
    //settings = SETTINGS_COLOR_PANEL (user_data);

    /* get devices */
    cd_client_get_devices (settings->client,
                           settings->cancellable,
                           color_settings_get_devices_cb,
                           settings);
}



static void
color_settings_dialog_init (GtkBuilder *builder)
{
    GObject *profile_add;

    ColorSettings *settings;
    settings = g_new0 (ColorSettings, 1);
    settings->cancellable = g_cancellable_new ();
    settings->devices = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);

    /* use a device client array */
    settings->client = cd_client_new ();
    g_signal_connect_data (settings->client, "device-added",
                           G_CALLBACK (color_settings_device_added_cb), settings, 0, 0);
    g_signal_connect_data (settings->client, "device-removed",
                           G_CALLBACK (color_settings_device_removed_cb), settings, 0, 0);

    profile_add = gtk_builder_get_object (builder, "profile-add");
    g_signal_connect (profile_add, "clicked", G_CALLBACK (color_settings_profile_add_cb), NULL);

    settings->label_no_devices = gtk_builder_get_object (builder, "label-no-devices");
    settings->box_devices = gtk_builder_get_object (builder, "box-devices");
    settings->grid = gtk_builder_get_object (builder, "grid");

    /* Devices ListBox */
    settings->frame_devices = gtk_builder_get_object (builder, "frame-devices");
    settings->list_box = GTK_LIST_BOX (gtk_list_box_new ());
    gtk_list_box_set_header_func (settings->list_box,
                              list_box_update_header_func,
                              settings, NULL);
    gtk_list_box_set_selection_mode (settings->list_box,
                                 GTK_SELECTION_SINGLE);
    gtk_list_box_set_activate_on_single_click (settings->list_box, TRUE);
    settings->list_box_selected_id =
        g_signal_connect (settings->list_box, "row-selected",
                          G_CALLBACK (color_settings_list_box_row_activated_cb),
                          settings);
    settings->list_box_activated_id =
    g_signal_connect (settings->list_box, "row-activated",
                      G_CALLBACK (color_settings_list_box_row_activated_cb),
                      settings);
    settings->list_box_size = gtk_size_group_new (GTK_SIZE_GROUP_VERTICAL);

    gtk_container_add (GTK_CONTAINER (settings->frame_devices), GTK_WIDGET (settings->list_box));
    gtk_widget_show_all (GTK_WIDGET (settings->list_box));

    /* Profiles ListBox */
    settings->label_no_profiles = gtk_builder_get_object (builder, "label-no-profiles");
    settings->box_profiles = gtk_builder_get_object (builder, "box-profiles");
    settings->frame_profiles = gtk_builder_get_object (builder, "frame-profiles");
    settings->profiles_list_box = GTK_LIST_BOX (gtk_list_box_new ());
    gtk_list_box_set_header_func (settings->profiles_list_box,
                              list_box_update_header_func,
                              settings, NULL);
    gtk_list_box_set_selection_mode (settings->profiles_list_box,
                                 GTK_SELECTION_SINGLE);
    gtk_list_box_set_activate_on_single_click (settings->profiles_list_box, TRUE);
    settings->profiles_list_box_selected_id =
        g_signal_connect (settings->profiles_list_box, "row-selected",
                          G_CALLBACK (color_settings_profiles_list_box_row_activated_cb),
                          settings);
    settings->profiles_list_box_activated_id =
    g_signal_connect (settings->profiles_list_box, "row-activated",
                  G_CALLBACK (color_settings_profiles_list_box_row_activated_cb),
                  settings);
    settings->profiles_list_box_size = gtk_size_group_new (GTK_SIZE_GROUP_VERTICAL);

    gtk_container_add (GTK_CONTAINER (settings->frame_profiles), GTK_WIDGET (settings->profiles_list_box));
    gtk_widget_show (GTK_WIDGET (settings->profiles_list_box));

    cd_client_connect (settings->client,
                       settings->cancellable,
                       color_settings_connect_cb,
                       settings);
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
        /* Initialize the dialog */
        color_settings_dialog_init (builder);

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
