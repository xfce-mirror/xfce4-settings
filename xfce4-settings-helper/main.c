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

#include <glib.h>
#include <gtk/gtk.h>

#include <xfconf/xfconf.h>
#include <libxfce4util/libxfce4util.h>

#include "pointers.h"
#include "xkb.h"
#include "accessx.h"
#include "keyboard-shortcuts.h"



static gboolean     opt_version = FALSE;
static gboolean     opt_debug = FALSE;
static GOptionEntry option_entries[] =
{
    { "version", 'v', 0, G_OPTION_ARG_NONE, &opt_version, N_("Version information"), NULL },
    { "debug", 'd', 0, G_OPTION_ARG_NONE, &opt_debug, N_("Start in debug mode (don't fork to the background)"), NULL },
    { NULL }
};



gint
main (gint argc, gchar **argv)
{
    GError  *error = NULL;
    GObject *pointer_helper;
    GObject *xkb_helper;
    GObject *accessx_helper;
    GObject *shortcuts_helper;
    pid_t    pid;

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

    /* daemonize the process when not running in debug mode */
    if (!opt_debug)
    {
        /* try to fork the process */
        pid = fork ();

        if (G_UNLIKELY (pid == -1))
        {
            /* show message and continue in normal mode */
            g_warning ("Failed to fork the process, starting in non-daemon mode");
        }
        else if (pid > 0)
        {
            /* succesfully created a fork, leave this instance */
            return EXIT_SUCCESS;
        }
    }

    /* initialize xfconf */
    if (!xfconf_init (&error))
    {
        /* print error and exit */
        g_error ("Failed to connect to xfconf daemon: %s.", error->message);
        g_error_free (error);

        return EXIT_FAILURE;
    }

    /* create the sub daemons */
    pointer_helper = g_object_new (XFCE_TYPE_POINTERS_HELPER, NULL);
    xkb_helper = g_object_new (XFCE_TYPE_XKB_HELPER, NULL);
    accessx_helper = g_object_new (XFCE_TYPE_ACCESSX_HELPER, NULL);
    shortcuts_helper = g_object_new (XFCE_TYPE_KEYBOARD_SHORTCUTS_HELPER, NULL);

    /* enter the main loop */
    gtk_main();

    /* release the sub daemons */
    g_object_unref (G_OBJECT (pointer_helper));
    g_object_unref (G_OBJECT (xkb_helper));
    g_object_unref (G_OBJECT (accessx_helper));
    g_object_unref (G_OBJECT (shortcuts_helper));

    /* shutdown xfconf */
    xfconf_shutdown ();

    return EXIT_SUCCESS;
}
