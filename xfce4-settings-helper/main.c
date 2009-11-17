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
#include <libxfce4ui/libxfce4ui.h>

#include "accessibility.h"
#include "pointers.h"
#include "keyboards.h"
#include "keyboard-layout.h"
#include "keyboard-shortcuts.h"
#include "workspaces.h"
#include "xfce-clipboard-manager.h"

#ifdef HAVE_XRANDR
#include "displays.h"
#endif

#define SELECTION_NAME  "_XFCE_SETTINGS_HELPER"

static GdkFilterReturn xfce_settings_helper_selection_watcher (GdkXEvent *xevt,
                                                               GdkEvent *evt,
                                                               gpointer user_data);


static XfceSMClient *sm_client = NULL;

static gboolean opt_version = FALSE;
static gboolean opt_debug = FALSE;
static GOptionEntry option_entries[] =
{
    { "version", 'V', 0, G_OPTION_ARG_NONE, &opt_version, N_("Version information"), NULL },
    { "debug", 'd', 0, G_OPTION_ARG_NONE, &opt_debug, N_("Start in debug mode (don't fork to the background)"), NULL },
    { NULL }
};



static void
signal_handler (gint signum,
                gpointer user_data)
{
    /* quit the main loop */
    gtk_main_quit ();
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



#ifdef GDK_WINDOWING_X11
static GdkFilterReturn
xfce_settings_helper_selection_watcher (GdkXEvent *xevt,
                                        GdkEvent *evt,
                                        gpointer user_data)
{
    Window xwin = GPOINTER_TO_UINT(user_data);
    XEvent *xe = (XEvent *)xevt;

    if (xe->type == SelectionClear && xe->xclient.window == xwin)
    {
        if (sm_client)
            xfce_sm_client_set_restart_style (sm_client, XFCE_SM_CLIENT_RESTART_NORMAL);
        gtk_main_quit ();
    }

    return GDK_FILTER_CONTINUE;
}
#endif

static gboolean
xfce_settings_helper_acquire_selection (gboolean force)
{
#ifdef GDK_WINDOWING_X11
    GdkDisplay *gdpy = gdk_display_get_default ();
    GtkWidget *invisible;
    Display *dpy = GDK_DISPLAY_XDISPLAY (gdpy);
    GdkWindow *rootwin = gdk_screen_get_root_window (gdk_display_get_screen (gdpy, 0));
    Window xroot = GDK_WINDOW_XID (rootwin);
    GdkAtom selection_atom;
    Atom selection_atom_x11;
    XClientMessageEvent xev;

    selection_atom = gdk_atom_intern (SELECTION_NAME, FALSE);
    selection_atom_x11 = gdk_x11_atom_to_xatom_for_display (gdpy, selection_atom);

    /* can't use gdk for the selection owner here because it returns NULL
     * if the selection owner is in another process */
    if (!force && XGetSelectionOwner (dpy, selection_atom_x11) != None)
        return FALSE;

    invisible = gtk_invisible_new ();
    gtk_widget_realize (invisible);
    gtk_widget_add_events (invisible, GDK_STRUCTURE_MASK | GDK_PROPERTY_CHANGE_MASK);

    if (!gdk_selection_owner_set_for_display (gdpy, invisible->window,
                                              selection_atom, GDK_CURRENT_TIME,
                                              TRUE))
    {
        g_critical ("Unable to get selection " SELECTION_NAME);
        gtk_widget_destroy (invisible);
        return FALSE;
    }

    /* but we can use gdk here since we only care if it's our window */
    if (gdk_selection_owner_get_for_display (gdpy, selection_atom) != invisible->window)
    {
        gtk_widget_destroy (invisible);
        return FALSE;
    }

    xev.type = ClientMessage;
    xev.window = xroot;
    xev.message_type = gdk_x11_get_xatom_by_name_for_display (gdpy, "MANAGER");
    xev.format = 32;
    xev.data.l[0] = CurrentTime;
    xev.data.l[1] = selection_atom_x11;
    xev.data.l[2] = GDK_WINDOW_XID (invisible->window);
    xev.data.l[3] = xev.data.l[4] = 0;

    XSendEvent (dpy, xroot, False, StructureNotifyMask, (XEvent *)&xev);

    gdk_window_add_filter (invisible->window,
                           xfce_settings_helper_selection_watcher,
                           GUINT_TO_POINTER (GDK_WINDOW_XID (invisible->window)));
#endif

    return TRUE;
}


gint
main (gint argc, gchar **argv)
{
    XfceClipboardManager *clipboard_daemon;
    GError               *error = NULL;
    GOptionContext       *context;
    gboolean              in_session;
    GObject              *pointer_helper;
    GObject              *keyboards_helper;
    GObject              *accessibility_helper;
    GObject              *shortcuts_helper;
    GObject              *keyboard_layout_helper;
#ifdef HAVE_XRANDR
    GObject              *displays_helper;
#endif
    GObject              *workspaces_helper;
    pid_t                 pid;
    guint                 i;
    const gint            signums[] = { SIGHUP, SIGINT, SIGQUIT, SIGTERM };

    /* setup translation domain */
    xfce_textdomain (GETTEXT_PACKAGE, LOCALEDIR, "UTF-8");

    /* create option context */
    context = g_option_context_new (NULL);
    g_option_context_add_main_entries (context, option_entries, GETTEXT_PACKAGE);
    g_option_context_add_group (context, gtk_get_option_group (FALSE));
    g_option_context_add_group (context, xfce_sm_client_get_option_group (argc, argv));

    /* initialize gtk */
    gtk_init (&argc, &argv);

    /* parse options */
    if (!g_option_context_parse (context, &argc, &argv, &error))
    {
        /* print error */
        g_print ("%s: %s.\n", G_LOG_DOMAIN, error->message);
        g_print (_("Type '%s --help' for usage."), G_LOG_DOMAIN);
        g_print ("\n");

        /* cleanup */
        g_error_free (error);
        g_option_context_free (context);

        return EXIT_FAILURE;
    }

    /* cleanup */
    g_option_context_free (context);

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

    /* connect to session always, even if we quit below.  this way the
     * session manager won't wait for us to time out. */
    sm_client = xfce_sm_client_get ();
    xfce_sm_client_set_restart_style (sm_client, XFCE_SM_CLIENT_RESTART_IMMEDIATELY);
    g_signal_connect (G_OBJECT (sm_client), "quit", G_CALLBACK (gtk_main_quit), NULL);
    if (!xfce_sm_client_connect (sm_client, &error) && error)
    {
        g_printerr ("Failed to connect to session manager: %s\n", error->message);
        g_error_free (error);
    }

    in_session = xfce_sm_client_is_resumed (sm_client);
    if (!xfce_settings_helper_acquire_selection (in_session))
    {
        g_printerr ("%s is already running\n", G_LOG_DOMAIN);
        g_object_unref (G_OBJECT (sm_client));
        return EXIT_FAILURE;
    }

    /* if we were restarted as part of the session, remove us from autostart */
    xfce_settings_helper_set_autostart_enabled (!in_session);

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

    /* create the sub daemons */
    pointer_helper = g_object_new (XFCE_TYPE_POINTERS_HELPER, NULL);
    keyboards_helper = g_object_new (XFCE_TYPE_KEYBOARDS_HELPER, NULL);
    accessibility_helper = g_object_new (XFCE_TYPE_ACCESSIBILITY_HELPER, NULL);
    shortcuts_helper = g_object_new (XFCE_TYPE_KEYBOARD_SHORTCUTS_HELPER, NULL);
    keyboard_layout_helper = g_object_new (XFCE_TYPE_KEYBOARD_LAYOUT_HELPER, NULL);
#ifdef HAVE_XRANDR
    displays_helper = g_object_new (XFCE_TYPE_DISPLAYS_HELPER, NULL);
#endif
    workspaces_helper = g_object_new (XFCE_TYPE_WORKSPACES_HELPER, NULL);

    /* Try to start the clipboard daemon */
    clipboard_daemon = xfce_clipboard_manager_new ();
    xfce_clipboard_manager_start (clipboard_daemon, NULL);

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
#ifdef HAVE_XRANDR
    g_object_unref (G_OBJECT (displays_helper));
#endif
    g_object_unref (G_OBJECT (workspaces_helper));

    /* Stop the clipboard daemon */
    xfce_clipboard_manager_stop (clipboard_daemon);

    /* shutdown xfconf */
    xfconf_shutdown ();

    /* release sm client */
    g_object_unref (G_OBJECT (sm_client));

    return EXIT_SUCCESS;
}
