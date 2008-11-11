/* $Id$ */
/*
 *  Copyright (c) 2008 Stephan Arts <stephan@xfce.org>
 *  Copyright (c) 2008 Nick Schermer <nick@xfce.org>
 *  Copyright (c) 2008 Jannis Pohlmann <jannis@xfce.org>
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
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_SIGNAL_H
#include <signal.h>
#endif
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif

#include <glib.h>
#include <gtk/gtk.h>

#ifdef GDK_WINDOWING_X11
#include <gdk/gdkx.h>
#include <X11/X.h>
#include <X11/Xlib.h>
#endif

#include <xfconf/xfconf.h>
#include <libxfce4util/libxfce4util.h>
#include <libxfcegui4/libxfcegui4.h>

#include "accessibility.h"
#include "displays.h"
#include "pointers.h"
#include "keyboards.h"
#include "keyboard-layout.h"
#include "keyboard-shortcuts.h"
#include "workspaces.h"



static gboolean opt_version = FALSE;
static gboolean opt_debug = FALSE;
static gchar   *opt_sm_client_id = NULL;
static GOptionEntry option_entries[] =
{
    { "version", 'V', 0, G_OPTION_ARG_NONE, &opt_version, N_("Version information"), NULL },
    { "debug", 'd', 0, G_OPTION_ARG_NONE, &opt_debug, N_("Start in debug mode (don't fork to the background)"), NULL },
    { "sm-client-id", 0, 0, G_OPTION_ARG_STRING, &opt_sm_client_id, N_("Client id used when resuming session"), NULL },
    { NULL }
};



static void
signal_handler (gint signum,
                gpointer user_data)
{
    /* quit the main loop */
    gtk_main_quit ();
}


static void
sm_client_die (gpointer client_data)
{
    signal_handler (SIGTERM, client_data);
}


static gboolean
xfce_settings_helper_set_autostart_enabled (gboolean enabled)
{
    gboolean ret = TRUE;
    XfceRc *rcfile = xfce_rc_config_open (XFCE_RESOURCE_CONFIG,
                                          "autostart/" AUTOSTART_FILENAME,
                                          FALSE);

    if (G_UNLIKELY (rcfile == NULL))
    {
        g_warning ("Failed to create per-user autostart directory");
        return FALSE;
    }

    xfce_rc_set_group (rcfile, "Desktop Entry");
    if (xfce_rc_read_bool_entry (rcfile, "Hidden", enabled) == enabled)
    {
        xfce_rc_write_bool_entry (rcfile, "Hidden", !enabled);
        xfce_rc_flush (rcfile);
    }

    if (xfce_rc_is_dirty (rcfile))
    {
        g_warning ("Failed to write autostart file");
        ret = FALSE;
    }

    xfce_rc_close (rcfile);

    return ret;
}


/* returns TRUE if we're now connected to the SM, FALSE otherwise */
static gboolean
xfce_settings_helper_connect_session (int argc,
                                      char **argv,
                                      const gchar *sm_client_id,
                                      gboolean debug_mode)
{
    SessionClient *sm_client;

    /* we can't be sure that the SM will save the session later, so we only
     * disable the autostart item if we're launching because we got *resumed*
     * from a previous session. */

    sm_client = client_session_new (argc, argv, NULL,
                                    debug_mode ? SESSION_RESTART_IF_RUNNING
                                               : SESSION_RESTART_IMMEDIATELY,
                                    40);
    sm_client->die = sm_client_die;
    if (sm_client_id)
        client_session_set_client_id (sm_client, sm_client_id);
    if (!session_init (sm_client))
    {
        g_warning ("Failed to connect to session manager");
        client_session_free (sm_client);
        xfce_settings_helper_set_autostart_enabled (TRUE);
        return FALSE;
    }

    if (sm_client_id && !g_ascii_strcasecmp (sm_client_id, sm_client->given_client_id))
    {
        /* we passed a client id, and got the same one back, which means
         * we were definitely restarted as a part of the session.  so
         * it's safe to disable the autostart item. */
        xfce_settings_helper_set_autostart_enabled (FALSE);
        return TRUE;
    }

    /* otherwise, let's just ensure the autostart item is enabled. */
    xfce_settings_helper_set_autostart_enabled (TRUE);

    return TRUE;
}


static gboolean
xfce_settings_helper_acquire_selection ()
{
#ifdef GDK_WINDOWING_X11
    GdkDisplay *gdpy = gdk_display_get_default ();
    Display *dpy = GDK_DISPLAY_XDISPLAY (gdpy);
    GdkWindow *rootwin = gdk_screen_get_root_window (gdk_display_get_screen (gdpy, 0));
    Window xroot = GDK_WINDOW_XID (rootwin);
    Window xwin;
    Atom selection_atom, manager_atom;
    XClientMessageEvent xev;

    xwin = XCreateSimpleWindow (dpy, xroot, -100, -100, 1, 1, 0, 0,
                                XBlackPixel (GDK_DISPLAY (), 0));
    XSelectInput (dpy, xwin, PropertyChangeMask | StructureNotifyMask);

    selection_atom = XInternAtom (dpy, "_XFCE_SETTINGS_HELPER", False);
    manager_atom = XInternAtom (dpy, "MANAGER", False);

    if (XGetSelectionOwner (dpy, selection_atom) != None)
    {
        XDestroyWindow (dpy, xwin);
        return FALSE;
    }

    XSetSelectionOwner (dpy, selection_atom, xwin, CurrentTime);

    if (XGetSelectionOwner (dpy, selection_atom) != xwin)
    {
        XDestroyWindow (dpy, xwin);
        return FALSE;
    }

    xev.type = ClientMessage;
    xev.window = xroot;
    xev.message_type = manager_atom;
    xev.format = 32;
    xev.data.l[0] = CurrentTime;
    xev.data.l[1] = selection_atom;
    xev.data.l[2] = xwin;
    xev.data.l[3] = xev.data.l[4] = 0;

    XSendEvent (dpy, xroot, False, StructureNotifyMask, (XEvent *)&xev);
#endif

    return TRUE;
}


gint
main (gint argc, gchar **argv)
{
    GError     *error = NULL;
    GObject    *pointer_helper;
    GObject    *keyboards_helper;
    GObject    *accessibility_helper;
    GObject    *shortcuts_helper;
    GObject    *keyboard_layout_helper;
    GObject    *displays_helper;
    GObject    *workspaces_helper;
    pid_t       pid;
    guint       i;
    const gint  signums[] = { SIGHUP, SIGINT, SIGQUIT, SIGTERM };

    /* setup translation domain */
    xfce_textdomain (GETTEXT_PACKAGE, LOCALEDIR, "UTF-8");

    /* initialize the gthread system */
    if (!g_thread_supported ())
        g_thread_init (NULL);

    /* initialize gtk */
    if(!gtk_init_with_args (&argc, &argv, "", option_entries, GETTEXT_PACKAGE, &error))
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

    if (!xfce_settings_helper_acquire_selection ())
    {
        g_printerr ("%s is already running\n", G_LOG_DOMAIN);
        return EXIT_FAILURE;
    }

    /* daemonize the process when not running in debug mode */
    if (!opt_debug)
    {
        /* try to fork the process */
        pid = fork ();

        if (G_UNLIKELY (pid == -1))
        {
            /* show message and continue in normal mode */
            g_warning ("Failed to fork the process: %s. Continuing in non-daemon mode.", g_strerror (errno));
        }
        else if (pid > 0)
        {
            /* succesfully created a fork, leave this instance */
            _exit (EXIT_SUCCESS);
        }
    }

    xfce_settings_helper_connect_session (argc, argv, opt_sm_client_id, opt_debug);

    /* create the sub daemons */
    pointer_helper = g_object_new (XFCE_TYPE_POINTERS_HELPER, NULL);
    keyboards_helper = g_object_new (XFCE_TYPE_KEYBOARDS_HELPER, NULL);
    accessibility_helper = g_object_new (XFCE_TYPE_ACCESSIBILITY_HELPER, NULL);
    shortcuts_helper = g_object_new (XFCE_TYPE_KEYBOARD_SHORTCUTS_HELPER, NULL);
    keyboard_layout_helper = g_object_new (XFCE_TYPE_KEYBOARD_LAYOUT_HELPER, NULL);
    displays_helper = g_object_new (XFCE_TYPE_DISPLAYS_HELPER, NULL);
    workspaces_helper = g_object_new (XFCE_TYPE_WORKSPACES_HELPER, NULL);

    /* setup signal handlers to properly quit the main loop */
    if (xfce_posix_signal_handler_init (NULL))
    {
        for (i = 0; i < G_N_ELEMENTS (signums); i++)
            xfce_posix_signal_handler_set_handler (signums[i], signal_handler, NULL, NULL);
    }

    /* enter the main loop */
    gtk_main();

    /* release the sub daemons */
    g_object_unref (G_OBJECT (pointer_helper));
    g_object_unref (G_OBJECT (keyboards_helper));
    g_object_unref (G_OBJECT (accessibility_helper));
    g_object_unref (G_OBJECT (shortcuts_helper));
    g_object_unref (G_OBJECT (keyboard_layout_helper));
    g_object_unref (G_OBJECT (displays_helper));
    g_object_unref (G_OBJECT (workspaces_helper));

    /* shutdown xfconf */
    xfconf_shutdown ();

    return EXIT_SUCCESS;
}
