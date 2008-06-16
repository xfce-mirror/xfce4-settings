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
 *
 *  XKB Extension code taken from the original mcs-keyboard-plugin written
 *  by Olivier Fourdan.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <X11/Xlib.h>

#include <X11/XKBlib.h>

#ifdef HAVE_XF86MISC
#include <X11/extensions/xf86misc.h>
#endif

#define HAVE_XKB

#include <string.h>

#include <glib.h>

#include <gtk/gtk.h>
#include <gdk/gdkx.h>

#include <libxfce4util/libxfce4util.h>
#include <xfconf/xfconf.h>
#include <libnotify/notify.h>

#include "xkb.h"

static gboolean xkbpresent = FALSE;

static XfconfChannel *xkb_channel;

static gboolean xkb_initialized = FALSE;

static void
set_repeat (int key, int auto_repeat_mode)
{
    XKeyboardControl values;
    values.auto_repeat_mode = auto_repeat_mode;

    gdk_flush ();
    gdk_error_trap_push ();
    if (key != -1)
    {
        values.key = key;
        XChangeKeyboardControl (GDK_DISPLAY (), KBKey | KBAutoRepeatMode, &values);
    }
    else
    {
        XChangeKeyboardControl (GDK_DISPLAY (), KBAutoRepeatMode, &values);
    }
    gdk_flush ();
    gdk_error_trap_pop ();
}

static void
set_repeat_rate (int delay, int rate)
{
#ifdef HAVE_XF86MISC
    XF86MiscKbdSettings values;
#endif

    g_return_if_fail (rate > 0);
    g_return_if_fail (delay > 0);

#ifdef HAVE_XF86MISC
    if (miscpresent)
    {
        gdk_flush ();
        gdk_error_trap_push ();
        XF86MiscGetKbdSettings (GDK_DISPLAY (), &values);
        if (delay != -1)
            values.delay = delay;
        if (rate != -1)
            values.rate = rate;
        XF86MiscSetKbdSettings (GDK_DISPLAY (), &values);
        gdk_flush ();
        gdk_error_trap_pop ();
    }
#endif

#ifdef HAVE_XKB
    if (xkbpresent)
    {
        XkbDescPtr xkb = XkbAllocKeyboard ();
        if (xkb)
        {
            gdk_error_trap_push ();
            XkbGetControls (GDK_DISPLAY (), XkbRepeatKeysMask, xkb);
            if (delay != -1)
                xkb->ctrls->repeat_delay = delay;
            if (rate != -1)
                xkb->ctrls->repeat_interval = 1000 / rate;
            XkbSetControls (GDK_DISPLAY (), XkbRepeatKeysMask, xkb);
            XFree (xkb);
            gdk_flush ();
            gdk_error_trap_pop ();
        }
        else
        {
            g_warning ("XkbAllocKeyboard() returned null pointer");
        }
    }
#endif
}



static void
cb_xkb_channel_property_changed(XfconfChannel *channel, const gchar *name, const GValue *value, gpointer user_data)
{
    if (!strcmp (name, "/Xkb/KeyRepeat"))
    {
        gboolean key_repeat = g_value_get_boolean (value);
        set_repeat (-1, key_repeat == TRUE?1:0);
    }

    /* TODO */
    if (!strcmp (name, "/Xkb/KeyRepeat/Delay"))
    {
        set_repeat_rate (g_value_get_int (value), -1);
    }
    if (!strcmp (name, "/Xkb/KeyRepeat/Rate"))
    {
        set_repeat_rate (-1, g_value_get_int (value));
    }
}

gint
xkb_notification_init (XfconfChannel *channel)
{
    g_return_val_if_fail (xkb_initialized == FALSE, 1);

    int xkbmajor = XkbMajorVersion, xkbminor = XkbMinorVersion;
    int xkbopcode, xkbevent, xkberror;
    xkb_channel = channel;

#ifdef DEBUG
    g_message ("Querying Xkb extension");
#endif
    if (XkbQueryExtension (GDK_DISPLAY (), &xkbopcode, &xkbevent, &xkberror, &xkbmajor, &xkbminor))
    {
#ifdef DEBUG
        g_message ("Xkb extension found");
#endif
        xkbpresent = TRUE;
    }
    else
    {
#ifdef DEBUG
        g_message ("Your X server does not support Xkb extension");
#endif
        xkbpresent = FALSE;
    }
#ifdef DEBUG
    g_warning ("This build doesn't include support for Xkb extension");
#endif
    
    g_signal_connect(G_OBJECT(channel), "property-changed", (GCallback)cb_xkb_channel_property_changed, NULL);

    xkb_initialized = TRUE;
    return xkbpresent;
}

