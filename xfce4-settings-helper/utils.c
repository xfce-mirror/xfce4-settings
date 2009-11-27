/*
 * Copyright (C) 2009 Nick Schermer <nick@xfce.org>
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtk.h>

#ifdef GDK_WINDOWING_X11
#include <gdk/gdkx.h>
#include <X11/X.h>
#include <X11/Xlib.h>
#endif

#include "utils.h"

gboolean
xfce_utils_selection_owner (const gchar   *selection_name,
                            gboolean       force,
                            GdkFilterFunc  filter_func)
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
    gboolean has_owner;

    g_return_val_if_fail (selection_name != NULL, FALSE);

    selection_atom = gdk_atom_intern (selection_name, FALSE);
    selection_atom_x11 = gdk_x11_atom_to_xatom_for_display (gdpy, selection_atom);

    /* can't use gdk for the selection owner here because it returns NULL
     * if the selection owner is in another process */
    if (!force)
    {
        gdk_error_trap_push ();
        has_owner = XGetSelectionOwner (dpy, selection_atom_x11) != None;
        gdk_flush ();
        gdk_error_trap_pop ();
        if (has_owner)
            return FALSE;
    }

    invisible = gtk_invisible_new ();
    gtk_widget_realize (invisible);
    gtk_widget_add_events (invisible, GDK_STRUCTURE_MASK | GDK_PROPERTY_CHANGE_MASK);

    if (!gdk_selection_owner_set_for_display (gdpy, invisible->window,
                                              selection_atom, GDK_CURRENT_TIME,
                                              TRUE))
    {
        g_critical ("Unable to get selection %s", selection_name);
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

    gdk_error_trap_push ();
    XSendEvent (dpy, xroot, False, StructureNotifyMask, (XEvent *)&xev);
    gdk_flush ();
    if (gdk_error_trap_pop ())
      g_critical ("Failed to send client event");

    if (filter_func != NULL)
        gdk_window_add_filter (invisible->window, filter_func, invisible);
#endif

    return TRUE;
}

