/* $Id$ */
/*
 *  Copyright (c) 2008 Nick Schermer <nick@xfce.org>
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

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <X11/Xlib.h>
#include <X11/extensions/randr.h>
#include <X11/extensions/Xrandr.h>
#include <X11/extensions/xf86vmode.h>

#include <glib.h>
#include <gtk/gtk.h>
#include <xfconf/xfconf.h>

#include "displays.h"



static void            xfce_displays_helper_class_init                     (XfceDisplaysHelperClass *klass);
static void            xfce_displays_helper_init                           (XfceDisplaysHelper      *helper);
static void            xfce_displays_helper_finalize                       (GObject                 *object);
static void            xfce_displays_helper_channel_property_changed       (XfconfChannel           *channel,
                                                                            const gchar             *property_name,
                                                                            const GValue            *value,
                                                                            XfceDisplaysHelper      *helper);



struct _XfceDisplaysHelperClass
{
    GObjectClass __parent__;
};

struct _XfceDisplaysHelper
{
    GObject  __parent__;

    /* xfconf channel */
    XfconfChannel *channel;
};



G_DEFINE_TYPE (XfceDisplaysHelper, xfce_displays_helper, G_TYPE_OBJECT);



static void
xfce_displays_helper_class_init (XfceDisplaysHelperClass *klass)
{
    GObjectClass *gobject_class;

    gobject_class = G_OBJECT_CLASS (klass);
    gobject_class->finalize = xfce_displays_helper_finalize;
}



static void
xfce_displays_helper_init (XfceDisplaysHelper *helper)
{
    /* open the channel */
    helper->channel = xfconf_channel_new ("displays");
    
    /* monitor channel changes */
    g_signal_connect (G_OBJECT (helper->channel), "property-changed", G_CALLBACK (xfce_displays_helper_channel_property_changed), helper);
}



static void
xfce_displays_helper_finalize (GObject *object)
{
    XfceDisplaysHelper *helper = XFCE_DISPLAYS_HELPER (object);

    /* release the channel */
    if (G_LIKELY (helper->channel))
        g_object_unref (G_OBJECT (helper->channel));

    (*G_OBJECT_CLASS (xfce_displays_helper_parent_class)->finalize) (object);
}



static void            
xfce_displays_helper_channel_property_changed (XfconfChannel      *channel,
                                               const gchar        *property_name,
                                               const GValue       *value,
                                               XfceDisplaysHelper *helper)
{
    
}
