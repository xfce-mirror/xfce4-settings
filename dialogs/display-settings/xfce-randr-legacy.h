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

#include <xfconf/xfconf.h>
#include <gdk/gdk.h>
#include <X11/extensions/Xrandr.h>

#ifndef __XFCE_RANDR_LEGACY_H__
#define __XFCE_RANDR_LEGACY_H__

#define XFCE_IS_STRING(string)               (string != NULL && *string != '\0')
#define XFCE_RANDR_LEGACY_CONFIG(legacy)     (legacy->config[legacy->active_screen])
#define XFCE_RANDR_LEGACY_ROTATION(legacy)   (legacy->rotation[legacy->active_screen])
#define XFCE_RANDR_LEGACY_RESOLUTION(legacy) (legacy->resolution[legacy->active_screen])
#define XFCE_RANDR_LEGACY_RATE(legacy)       (legacy->rate[legacy->active_screen])

typedef struct _XfceRandrLegacy XfceRandrLegacy;
typedef struct _XfceRotation    XfceRotation;

struct _XfceRandrLegacy
{
    /* display for this randr config */
    GdkDisplay              *display;

    /* randr screen configs for each screen */
    XRRScreenConfiguration **config;

    /* selected screen in the list */
    gint                     active_screen;

    /* total number of screens */
    gint                     num_screens;

    /* selected settings for each screen */
    SizeID                  *resolution;
    gshort                  *rate;
    Rotation                *rotation;
};

struct _XfceRotation
{
    Rotation     rotation;
    const gchar *name;
};



XfceRandrLegacy *xfce_randr_legacy_new    (GdkDisplay      *display,
                                           GError         **error);

void             xfce_randr_legacy_free   (XfceRandrLegacy *legacy);

void             xfce_randr_legacy_reload (XfceRandrLegacy *legacy);

void             xfce_randr_legacy_save   (XfceRandrLegacy *legacy,
                                           const gchar     *scheme,
                                           XfconfChannel   *channel);

void             xfce_randr_legacy_load   (XfceRandrLegacy *legacy,
                                           const gchar     *scheme,
                                           XfconfChannel   *channel);

#endif /* !__XFCE_RANDR_LEGACY_H__ */
