/* $Id$ */
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

#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <X11/Xlib.h>
#include <X11/XKBlib.h>

#ifdef HAVE_XF86MISC
#include <X11/extensions/xf86misc.h>
#endif

#include <glib.h>
#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <xfconf/xfconf.h>
#include <libxfce4util/libxfce4util.h>

#include <xfce4-settings-helper/xkb.h>



static void            xfce_xkb_helper_class_init                     (XfceXkbHelperClass *klass);
static void            xfce_xkb_helper_init                           (XfceXkbHelper      *helper);
static void            xfce_xkb_helper_finalize                       (GObject            *object);
static void            xfce_xkb_helper_set_auto_repeat_mode           (XfceXkbHelper      *helper);
static void            xfce_xkb_helper_set_repeat_rate                (XfceXkbHelper      *helper);
static void            xfce_xkb_helper_channel_property_changed       (XfconfChannel      *channel,
                                                                       const gchar        *property_name,
                                                                       const GValue       *value,
                                                                       XfceXkbHelper      *helper);



struct _XfceXkbHelperClass
{
    GObjectClass __parent__;
};

struct _XfceXkbHelper
{
    GObject  __parent__;

    /* xfconf channel */
    XfconfChannel      *channel;

    /* if xf86misc is present */
    guint               has_xf86misc : 1;
};



G_DEFINE_TYPE (XfceXkbHelper, xfce_xkb_helper, G_TYPE_OBJECT);



static void
xfce_xkb_helper_class_init (XfceXkbHelperClass *klass)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = xfce_xkb_helper_finalize;
}



static void
xfce_xkb_helper_init (XfceXkbHelper *helper)
{
    gint dummy;

    /* init */
    helper->channel = NULL;
    helper->has_xf86misc = FALSE;

    if (XkbQueryExtension (GDK_DISPLAY (), &dummy, &dummy, &dummy, &dummy, &dummy))
    {
#ifdef HAVE_XF86MISC
        /* chek for xf86misc */
        helper->has_xf86misc = XF86MiscQueryVersion (GDK_DISPLAY (), &dummy, &dummy);
#endif

        /* open the channel */
        helper->channel = xfconf_channel_new ("xkb");

        /* monitor channel changes */
        g_signal_connect (G_OBJECT (helper->channel), "property-changed", G_CALLBACK (xfce_xkb_helper_channel_property_changed), helper);

        /* load settings */
        xfce_xkb_helper_set_auto_repeat_mode (helper);
        xfce_xkb_helper_set_repeat_rate (helper);
    }
    else
    {
        /* warning */
        g_critical ("Failed to initialize the Xkb extension.");
    }
}



static void
xfce_xkb_helper_finalize (GObject *object)
{
    XfceXkbHelper *helper = XFCE_XKB_HELPER (object);

    /* release the channel */
    if (G_LIKELY (helper->channel))
        g_object_unref (G_OBJECT (helper->channel));

    (*G_OBJECT_CLASS (xfce_xkb_helper_parent_class)->finalize) (object);
}



static void
xfce_xkb_helper_set_auto_repeat_mode (XfceXkbHelper *helper)
{
    XKeyboardControl values;
    gboolean         repeat;

    /* load setting */
    repeat = xfconf_channel_get_bool (helper->channel, "/Xkb/KeyRepeat", FALSE);

    /* flush and avoid crashes on x errors */
    gdk_flush ();
    gdk_error_trap_push ();

    /* set key repeat */
    values.auto_repeat_mode = repeat ? 1 : 0;

    /* set key repeat */
    XChangeKeyboardControl (GDK_DISPLAY (), KBAutoRepeatMode, &values);

    /* flush errors and pop trap */
    gdk_flush ();
    gdk_error_trap_pop ();
}



static void
xfce_xkb_helper_set_repeat_rate (XfceXkbHelper *helper)
{
#ifdef HAVE_XF86MISC
    XF86MiscKbdSettings values;
#endif
    XkbDescPtr          xkb;
    gint                delay, rate;

    /* load settings */
    delay = xfconf_channel_get_int (helper->channel, "/Xkb/KeyRepeat/Delay", 0);
    rate = xfconf_channel_get_int (helper->channel, "/Xkb/KeyRepeat/Rate", 0);

    /* flush and avoid crashes on x errors */
    gdk_flush ();
    gdk_error_trap_push ();

#ifdef HAVE_XF86MISC
    /* update the xkb misc keyboard delay and rate */
    if (G_LIKELY (helper->has_xf86misc ))
    {
        XF86MiscGetKbdSettings (GDK_DISPLAY (), &values);
        values.delay = delay;
        values.rate = rate;
        XF86MiscSetKbdSettings (GDK_DISPLAY (), &values);
    }
#endif

    /* allocate xkb structure */
    xkb = XkbAllocKeyboard ();
    if (G_LIKELY (xkb))
    {
        /* load controls */
        XkbGetControls (GDK_DISPLAY (), XkbRepeatKeysMask, xkb);

        /* set new values */
        xkb->ctrls->repeat_delay = delay;
        xkb->ctrls->repeat_interval = 1000 / rate;

        /* set updated controls */
        XkbSetControls (GDK_DISPLAY (), XkbRepeatKeysMask, xkb);

        /* cleanup */
        XFree (xkb);
    }

    /* flush errors and pop trap */
    gdk_flush ();
    gdk_error_trap_pop ();
}



static void
xfce_xkb_helper_channel_property_changed (XfconfChannel *channel,
                                          const gchar   *property_name,
                                          const GValue  *value,
                                          XfceXkbHelper *helper)
{
    g_return_if_fail (helper->channel == channel);

    if (strcmp (property_name, "/Xkb/KeyRepeat") == 0)
    {
        /* update auto repeat mode */
        xfce_xkb_helper_set_auto_repeat_mode (helper);
    }
    else if (strcmp (property_name, "/Xkb/KeyRepeat/Delay") == 0
             || strcmp (property_name, "/Xkb/KeyRepeat/Rate") == 0)
    {
        /* update repeat rate */
        xfce_xkb_helper_set_repeat_rate (helper);
    }
}
