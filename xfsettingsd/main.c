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

#include "registry.h"

#define XF_DEBUG(str) \
    if (debug) g_print (str)

static gboolean version = FALSE;
static gboolean force_replace = FALSE;
static gboolean running = FALSE;
static gboolean debug = FALSE;

static GList *registries = NULL;

static GOptionEntry entries[] =
{
    {    "version", 'V', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE, &version,
        N_("Version information"),
        NULL
    },
    {    "verbose", 'v', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE, &version,
        N_("Verbose output"),
        NULL
    },
    {    "force", 'f', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE, &force_replace,
        N_("Replace running xsettings daemon (if any)"),
        NULL
    },
    {    "debug", 'd', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE, &debug,
        N_("Start in debug mode (don't fork to the background)"),
        NULL
    },
    { NULL }
};


static GdkFilterReturn
manager_event_filter (GdkXEvent *xevent,
                      GdkEvent *event,
                      gpointer data)
{
    XSettingsRegistry *registry = XSETTINGS_REGISTRY(data);

    if (xsettings_registry_process_event(registry, xevent))
    {
        g_object_unref(G_OBJECT(registry));
        registries = g_list_remove(registries, registry);
        if(!registries)
            gtk_main_quit();
        return GDK_FILTER_REMOVE;
    }
    else
    {
        return GDK_FILTER_CONTINUE;
    }
}

/**
 * settings_daemon_check_running:
 * @display: X11 Display object
 * @screen: X11 Screen number
 *
 * Return value: TRUE if an XSETTINGS daemon is already running
 */
static gboolean
settings_daemon_check_running (Display *display, gint screen)
{
    Atom atom;
    gchar buffer[256];

    g_snprintf(buffer, sizeof(buffer), "_XSETTINGS_S%d", screen);
    atom = XInternAtom((Display *)display, buffer, False);

    if (XGetSelectionOwner((Display *)display, atom))
    {
        return TRUE;
    }
    else
        return FALSE;
}

int
main(int argc, char **argv)
{
    GError *cli_error = NULL;
    GdkDisplay *gdpy;
    gint n_screens, screen;
    gboolean keep_running = FALSE;
    XfconfChannel *xsettings_channel;

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
        g_print("xfsettingsd %s\n", PACKAGE_VERSION);
        return 0;
    }

    if(!xfconf_init(&cli_error))
    {
        g_printerr("Failed to connect to Xfconf daemon: %s\n",
                   cli_error->message);
        return 1;
    }

    xsettings_channel = xfconf_channel_new("xsettings");

    gdpy = gdk_display_get_default();
    n_screens = gdk_display_get_n_screens(gdpy);

    for(screen = 0; screen < n_screens; ++screen)
    {
        XSettingsRegistry *registry;

        running = settings_daemon_check_running(GDK_DISPLAY_XDISPLAY(gdpy),
                                                screen);

        if (running)
        {
            XF_DEBUG("XSETTINGS Daemon running\n");
            if (force_replace)
            {
                XF_DEBUG("Replacing XSETTINGS daemon\n");
                keep_running = TRUE;
            }
            else
            {
                continue;
            }
        }

        XF_DEBUG("Initializing...\n");

        registry = xsettings_registry_new(xsettings_channel,
                                          GDK_DISPLAY_XDISPLAY(gdpy),
                                          screen);
        registries = g_list_append(registries, registry);
        
        xsettings_registry_load(registry, debug);

        xsettings_registry_notify(registry);

        gdk_window_add_filter(NULL, manager_event_filter, registry);

        keep_running = TRUE;
    }

    if(!keep_running)
    {
        XF_DEBUG("Not replacing existing XSETTINGS manager\n");
        return 1;
    }

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
