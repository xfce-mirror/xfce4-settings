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

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <X11/Xlib.h>

#include <glib.h>

#include <gtk/gtk.h>
#include <gdk/gdkx.h>

#include <libxfce4util/libxfce4util.h>
#include <xfconf/xfconf.h>
#include <libnotify/notify.h>

#include "xkb.h"
#include "accessx.h"

#define XF_DEBUG(str) \
    if (debug) g_print (str)

static gboolean version = FALSE;
static gboolean force_replace = FALSE;
static gboolean running = FALSE;
static gboolean debug = FALSE;

static GList *registries = NULL;

static GOptionEntry entries[] =
{
    {    "version", 'v', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE, &version,
        N_("Version information"),
        NULL
    },
    {    "debug", 'd', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE, &debug,
        N_("Start in debug mode (don't fork to the background)"),
        NULL
    },
    { NULL }
};


int
main(int argc, char **argv)
{
    GError *cli_error = NULL;
    GdkDisplay *gdpy;
    gint n_screens, screen;
    gboolean keep_running = FALSE;
    XfconfChannel *accessx_channel, *xkb_channel;

    xfce_textdomain(GETTEXT_PACKAGE, LOCALEDIR, "UTF-8");

    if(!gtk_init_with_args(&argc, &argv, _(""), entries, PACKAGE, &cli_error))
    {
        if (cli_error != NULL)
        {
            g_print (_("%s: %s\nTry %s --help to see a full list of available command line options.\n"), PACKAGE, cli_error->message, PACKAGE_NAME);
            g_error_free (cli_error);
            return 1;
        }
    }

    if(version)
    {
        g_print("xfce-xkbd %s\n", PACKAGE_VERSION);
        return 0;
    }

    if(!xfconf_init(&cli_error))
    {
        g_printerr("Failed to connect to Xfconf daemon: %s\n",
                   cli_error->message);
        return 1;
    }

    notify_init("xfce4-settings-helper");

    accessx_channel = xfconf_channel_new("accessx");
    xkb_channel = xfconf_channel_new("xkb");


    accessx_notification_init(accessx_channel);
    xkb_notification_init(accessx_channel);

    if(!debug) /* If not in debug mode, fork to background */
    {
        if(!fork())
        {
            gtk_main();
    
            xfconf_shutdown();
        }
    }
    else
    {
        gtk_main();

        xfconf_shutdown();
    }

    return 0;
}
