/* $Id$ */
/*
 *  xfce4-settings-manager
 *
 *  Copyright (c) 2008 Brian Tarricone <bjt23@cornell.edu>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License ONLY.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#include <gtk/gtk.h>
#include <xfconf/xfconf.h>
#include <libxfce4util/libxfce4util.h>

#include "xfce-settings-manager-dialog.h"

static gboolean opt_version = FALSE;

static GOptionEntry option_entries[] = {
    { "version", 'V', 0, G_OPTION_ARG_NONE, &opt_version, 
      N_("Version information"), NULL },
    { NULL }
};

int
main(int argc,
     char **argv)
{
    GtkWidget *dialog;
    GError *error = NULL;

    xfce_textdomain(GETTEXT_PACKAGE, LOCALEDIR, "UTF-8");

    if(!gtk_init_with_args(&argc, &argv, "", option_entries, GETTEXT_PACKAGE, &error)) {
        if(G_LIKELY(error)) {
            g_print("%s: %s.\n", G_LOG_DOMAIN, error->message);
            g_print(_("Type '%s --help' for usage."), G_LOG_DOMAIN);
            g_print("\n");

            g_error_free(error);
        } else {
            g_error("Unable to open display.");
        }
        return EXIT_FAILURE;
    }

    if(G_UNLIKELY(opt_version)) {
        g_print("%s %s (Xfce %s)\n\n", G_LOG_DOMAIN, PACKAGE_VERSION, xfce_version_string());
        g_print("%s\n", "Copyright (c) 2008");
        g_print("\t%s\n\n", _("The Xfce development team. All rights reserved."));
        g_print(_("Please report bugs to <%s>."), PACKAGE_BUGREPORT);
        g_print("\n");
        return EXIT_SUCCESS;
    }

    if(G_UNLIKELY(!xfconf_init (&error))) {
        if(G_LIKELY(error != NULL)) {
            g_error("%s: Failed to initialize xfconf: %s.\n", G_LOG_DOMAIN, 
                    error->message);
            g_error_free(error);
        } else {
            g_error("Failed to initialize xfconf.");
        }
        return EXIT_FAILURE;
    }

    dialog = xfce_settings_manager_dialog_new();
    gtk_widget_show(dialog);
    g_signal_connect(G_OBJECT(dialog), "response",
                     G_CALLBACK(gtk_main_quit), NULL);

    gtk_main();

    gtk_widget_destroy(dialog);

    xfconf_shutdown();

    return EXIT_SUCCESS;
}
