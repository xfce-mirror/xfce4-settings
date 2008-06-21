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

#include <config.h>
#include <string.h>

#include <glib.h>

#if defined(GETTEXT_PACKAGE)
#include <glib/gi18n-lib.h>
#else
#include <glib/gi18n.h>
#endif

#include <gtk/gtk.h>
#include <glade/glade.h>

#include <libxfcegui4/libxfcegui4.h>
#include <xfconf/xfconf.h>
#include "mouse-dialog_glade.h"

typedef struct {
    GtkWidget *slave;
    XfconfChannel *channel;
} PropertyPair;

gboolean version = FALSE;

static GOptionEntry entries[] =
{
    {    "version", 'v', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE, &version,
        N_("Version information"),
        NULL
    },
    { NULL }
};

GtkWidget *
mouse_settings_dialog_new_from_xml (GladeXML *gxml)
{
    XfconfChannel *xsettings_channel = xfconf_channel_new("xsettings");

    GtkWidget *dialog = glade_xml_get_widget (gxml, "mouse-settings-dialog");
    gtk_widget_show_all(dialog);
    gtk_widget_hide(dialog);
    return dialog;
}

int
main(int argc, char **argv)
{
    GladeXML *gxml;
    XfconfChannel *channel;
    GError *cli_error = NULL;
    gchar *data_dirs = NULL;

    #ifdef ENABLE_NLS
    bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
    bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
    textdomain (GETTEXT_PACKAGE);
    #endif

    if(!gtk_init_with_args(&argc, &argv, _("."), entries, PACKAGE, &cli_error))
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
        g_print("%s\n", PACKAGE_STRING);
        return 0;
    }

    xfconf_init(NULL);

    gxml = glade_xml_new_from_buffer (mouse_dialog_glade,
                                      mouse_dialog_glade_length,
                                      NULL, NULL);

    GtkWidget *dialog = mouse_settings_dialog_new_from_xml (gxml);

    gtk_dialog_run(GTK_DIALOG(dialog));

    xfconf_shutdown();

    return 0;
}
