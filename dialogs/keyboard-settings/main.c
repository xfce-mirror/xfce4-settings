/* vi:set sw=2 sts=2 ts=2 et ai: */
/*-
 * Copyright (c) 2008 Jannis Pohlmann <jannis@xfce.org>.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifdef HAVE_XFCE_REVISION_H
#include "xfce-revision.h"
#endif

#include "xfce-keyboard-settings.h"

#include <gtk/gtk.h>
#include <libxfce4ui/libxfce4ui.h>
#include <xfconf/xfconf.h>

#ifdef ENABLE_X11
#include <gdk/gdkx.h>
#endif



static gint opt_socket_id = 0;
static gboolean opt_version = FALSE;
static gboolean opt_shortcuts = FALSE;
static gchar *opt_shortcuts_command = NULL;
static gboolean
parse_shortcuts_option (const gchar *option_name,
                        const gchar *value,
                        gpointer data,
                        GError **error)
{
  opt_shortcuts = TRUE;
  opt_shortcuts_command = g_strdup (value);
  return TRUE;
}
static GOptionEntry entries[] = {
  { "socket-id", 's', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_INT, &opt_socket_id, N_ ("Settings manager socket"), N_ ("SOCKET ID") },
  { "version", 'v', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE, &opt_version, N_ ("Version information"), NULL },
  { "shortcuts", '\0', G_OPTION_FLAG_IN_MAIN | G_OPTION_FLAG_OPTIONAL_ARG, G_OPTION_ARG_CALLBACK, parse_shortcuts_option, N_ ("Switch to the shortcuts tab, and optionally open the add shortcut dialog if a command is provided"), NULL },
  { NULL }
};



static void
keyboard_settings_dialog_response (GtkWidget *dialog,
                                   gint response_id)
{
  if (response_id == GTK_RESPONSE_HELP)
    xfce_dialog_show_help_with_version (GTK_WINDOW (dialog), "xfce4-settings", "keyboard",
                                        NULL, VERSION_SHORT);
  else
    gtk_main_quit ();
}



int
main (int argc,
      char **argv)
{
  XfceKeyboardSettings *settings;
  GtkWidget *dialog;
  GError *error = NULL;

  /* Set up translation domain */
  xfce_textdomain (GETTEXT_PACKAGE, LOCALEDIR, "UTF-8");

  /* Initialize GTK+ and parse command line options */
  if (G_UNLIKELY (!gtk_init_with_args (&argc, &argv, NULL, entries, PACKAGE, &error)))
    {
      if (G_LIKELY (error != NULL))
        {
          g_print ("%s: %s.\n", G_LOG_DOMAIN, error->message);
          g_print (_("Type '%s --help' for usage."), G_LOG_DOMAIN);
          g_print ("\n");

          g_error_free (error);
        }
      else
        g_critical (_("Unable to initialize GTK+."));

      return EXIT_FAILURE;
    }

  /* Print version info and quit whe the user entered --version or -v */
  if (G_UNLIKELY (opt_version))
    {
      g_print ("%s %s (Xfce %s)\n\n", G_LOG_DOMAIN, VERSION_FULL, xfce_version_string ());
      g_print ("%s\n", "Copyright (c) 2008-" COPYRIGHT_YEAR);
      g_print ("\t%s\n\n", _("The Xfce development team. All rights reserved."));
      g_print (_("Please report bugs to <%s>."), PACKAGE_BUGREPORT);
      g_print ("\n");

      return EXIT_SUCCESS;
    }

  /* Initialize xfconf */
  if (G_UNLIKELY (!xfconf_init (&error)))
    {
      g_critical (_("Failed to connect to xfconf daemon. Reason: %s"), error->message);
      g_error_free (error);

      return EXIT_FAILURE;
    }

  /* Create the settings object */
  settings = xfce_keyboard_settings_new ();

  if (G_UNLIKELY (settings == NULL))
    {
      g_critical (_("Could not create the settings dialog."));
      xfconf_shutdown ();
      return EXIT_FAILURE;
    }

  DBG ("opt_socket_id = %i", opt_socket_id);

#ifdef ENABLE_X11
  if (opt_socket_id != 0 && GDK_IS_X11_DISPLAY (gdk_display_get_default ()))
    {
      /* Embedd the settings dialog into the given socket ID */
      GtkWidget *plug = xfce_keyboard_settings_create_plug (settings, opt_socket_id);
      g_signal_connect (plug, "delete-event", G_CALLBACK (gtk_main_quit), NULL);
      if (opt_shortcuts)
        xfce_keyboard_settings_switch_to_shortcuts_tab (settings, opt_shortcuts_command);

      /* Stop startup notification */
      gdk_notify_startup_complete ();

      /* To prevent the settings dialog to be saved in the session */
      gdk_x11_set_sm_client_id ("FAKE ID");

      /* Enter the main loop */
      gtk_main ();
    }
  else
#endif
    {
      /* Create and run the settings dialog */
      dialog = xfce_keyboard_settings_create_dialog (settings);

      g_signal_connect (dialog, "response",
                        G_CALLBACK (keyboard_settings_dialog_response), NULL);
      gtk_window_present (GTK_WINDOW (dialog));
      if (opt_shortcuts)
        xfce_keyboard_settings_switch_to_shortcuts_tab (settings, opt_shortcuts_command);

#ifdef ENABLE_X11
      if (GDK_IS_X11_DISPLAY (gdk_display_get_default ()))
        {
          /* To prevent the settings dialog to be saved in the session */
          gdk_x11_set_sm_client_id ("FAKE ID");
        }
#endif

      gtk_main ();
    }

  g_object_unref (settings);
  g_free (opt_shortcuts_command);
  xfconf_shutdown ();

  return EXIT_SUCCESS;
}
