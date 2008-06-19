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

#include <gtk/gtk.h>
#include <libxfce4util/libxfce4util.h>

#include "xfce-settings-manager-dialog.h"

int
main(int argc,
     char **argv)
{
    GtkWidget *dialog;

    xfce_textdomain(GETTEXT_PACKAGE, LOCALEDIR, "UTF-8");

    gtk_init(&argc, &argv);

    dialog = xfce_settings_manager_dialog_new();
    gtk_widget_show(dialog);
    g_signal_connect(G_OBJECT(dialog), "response",
                     G_CALLBACK(gtk_main_quit), NULL);

    gtk_main();

    return 0;
}
