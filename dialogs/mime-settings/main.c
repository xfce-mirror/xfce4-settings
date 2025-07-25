/*
 * Copyright (C) 2012 Nick Schermer <nick@xfce.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifdef HAVE_XFCE_REVISION_H
#include "xfce-revision.h"
#endif

#include "xfce-mime-window.h"

#include <gtk/gtk.h>
#include <libxfce4ui/libxfce4ui.h>
#include <libxfce4util/libxfce4util.h>
#include <xfconf/xfconf.h>

#ifdef ENABLE_X11
#include <gdk/gdkx.h>
#include <gtk/gtkx.h>
#endif



static gint opt_socket_id = 0;
static gboolean opt_version = FALSE;
static GOptionEntry entries[] = {
  { "socket-id", 's', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_INT, &opt_socket_id, N_ ("Settings manager socket"), N_ ("SOCKET ID") },
  { "version", 'v', 0, G_OPTION_ARG_NONE, &opt_version, N_ ("Version information"), NULL },
  { NULL }
};



static void
mime_window_dialog_response (GtkWidget *dialog,
                             gint response_id)
{
  if (response_id == GTK_RESPONSE_HELP)
    xfce_dialog_show_help_with_version (GTK_WINDOW (dialog), "xfce4-settings", "mime",
                                        NULL, VERSION_SHORT);
  else
    gtk_main_quit ();
}



gint
main (gint argc,
      gchar **argv)
{
  XfceMimeWindow *window;
  GtkWidget *dialog;
  GError *error = NULL;

  /* setup translation domain */
  xfce_textdomain (GETTEXT_PACKAGE, LOCALEDIR, "UTF-8");

  /* initialize Gtk+ */
  if (!gtk_init_with_args (&argc, &argv, NULL, entries, PACKAGE, &error))
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
      g_print ("%s %s (Xfce %s)\n\n", G_LOG_DOMAIN, VERSION_FULL, xfce_version_string ());
      g_print ("%s\n", "Copyright (c) 2008-" COPYRIGHT_YEAR);
      g_print ("\t%s\n\n", _("The Xfce development team. All rights reserved."));
      g_print (_("Please report bugs to <%s>."), PACKAGE_BUGREPORT);
      g_print ("\n");

      return EXIT_SUCCESS;
    }

  if (!xfconf_init (&error))
    {
      g_critical ("Failed to initialize xfconf: %s", error->message);
      g_error_free (error);
    }

  /* Create the window object */
  window = xfce_mime_window_new ();

  if (G_UNLIKELY (window == NULL))
    {
      g_error (_("Could not create the mime dialog."));
      xfconf_shutdown ();
      return EXIT_FAILURE;
    }

  DBG ("opt_socket_id = %i", opt_socket_id);

#ifdef ENABLE_X11
  if (opt_socket_id != 0 && GDK_IS_X11_DISPLAY (gdk_display_get_default ()))
    {
      /* Embedd the settings dialog into the given socket ID */
      GtkWidget *plug = xfce_mime_window_create_plug (window, opt_socket_id);
      g_signal_connect (plug, "delete-event", G_CALLBACK (gtk_main_quit), NULL);

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
      dialog = xfce_mime_window_create_dialog (window);

      g_signal_connect (dialog, "response",
                        G_CALLBACK (mime_window_dialog_response), NULL);
      gtk_window_present (GTK_WINDOW (dialog));
#ifdef ENABLE_X11
      /* To prevent the settings dialog to be saved in the session */
      if (GDK_IS_X11_DISPLAY (gdk_display_get_default ()))
        gdk_x11_set_sm_client_id ("FAKE ID");
#endif
      gtk_main ();
    }

  xfconf_shutdown ();

  return EXIT_SUCCESS;
}
