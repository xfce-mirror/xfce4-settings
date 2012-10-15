/*
 *  Copyright (c) 2008 Nick Schermer <nick@xfce.org>
 *  Copyright (C) 2010 Lionel Le Folgoc <lionel@lefolgoc.net>
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
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <xfconf/xfconf.h>
#include <gdk/gdk.h>
#include <X11/extensions/Xrandr.h>

#ifndef __XFCE_RANDR_H__
#define __XFCE_RANDR_H__

#define XFCE_RANDR_EVENT_BASE(randr)      (randr->event_base)
#define XFCE_RANDR_MODE(randr)            (randr->mode[randr->active_output])
#define XFCE_RANDR_SUPPORTED_MODES(randr) (randr->modes[randr->active_output])
#define XFCE_RANDR_ROTATION(randr)        (randr->rotation[randr->active_output])
#define XFCE_RANDR_ROTATIONS(randr)       (randr->rotations[randr->active_output])
#define XFCE_RANDR_OUTPUT_INFO(randr)     (randr->output_info[randr->active_output])
#define XFCE_RANDR_POS_X(randr)           (randr->position[randr->active_output].x)
#define XFCE_RANDR_POS_Y(randr)           (randr->position[randr->active_output].y)
#define XFCE_RANDR_ROTATIONS_MASK         (RR_Rotate_0|RR_Rotate_90|RR_Rotate_180|RR_Rotate_270)
#define XFCE_RANDR_REFLECTIONS_MASK       (RR_Reflect_X|RR_Reflect_Y)

/* check for randr 1.3 or better */
#if RANDR_MAJOR > 1 || (RANDR_MAJOR == 1 && RANDR_MINOR >= 3)
#define HAS_RANDR_ONE_POINT_THREE
#else
#undef HAS_RANDR_ONE_POINT_THREE
#endif

typedef struct _XfceRandr          XfceRandr;
typedef struct _XfceOutputPosition XfceOutputPosition;
typedef struct _XfceRRMode         XfceRRMode;
typedef struct _XfceRelation       XfceRelation;
typedef struct _XfceRotation       XfceRotation;
typedef enum   _XfceOutputStatus   XfceOutputStatus;
typedef enum   _XfceOutputRelation XfceOutputRelation;

enum _XfceOutputStatus
{
    XFCE_OUTPUT_STATUS_PRIMARY,
    XFCE_OUTPUT_STATUS_SECONDARY
};

enum _XfceOutputRelation
{
    XFCE_RANDR_PLACEMENT_MIRROR,
    XFCE_RANDR_PLACEMENT_UP,
    XFCE_RANDR_PLACEMENT_DOWN,
    XFCE_RANDR_PLACEMENT_RIGHT,
    XFCE_RANDR_PLACEMENT_LEFT
};

struct _XfceOutputPosition
{
    gint x;
    gint y;
};

struct _XfceRRMode
{
    RRMode  id;
    guint   width;
    guint   height;
    gdouble rate;
};

struct _XfceRelation
{
    XfceOutputRelation  relation;
    const gchar        *name;
};

struct _XfceRotation
{
    Rotation     rotation;
    const gchar *name;
};

struct _XfceRandr
{
    /* xrandr 1.3 capable */
    gint                 has_1_3;

    /* display for this randr config */
    GdkDisplay          *display;

    /* event base for notifications */
    gint                 event_base;

    /* screen resource for this display */
    XRRScreenResources  *resources;

    /* the active selected layout */
    guint                active_output;

    /* number of connected outputs */
    guint                noutput;

    /* cache for the output/mode info */
    XRROutputInfo      **output_info;
    XfceRRMode         **modes;

    /* modes common to all connected outputs */
    RRMode              *clone_modes;

    /* selected settings for all connected outputs */
    RRMode              *mode;
    Rotation            *rotation;
    Rotation            *rotations;
    XfceOutputPosition  *position;
    XfceOutputStatus    *status;
};



XfceRandr  *xfce_randr_new             (GdkDisplay    *display,
                                        GError       **error);

void        xfce_randr_free            (XfceRandr     *randr);

void        xfce_randr_reload          (XfceRandr     *randr);

void        xfce_randr_save_output     (XfceRandr     *randr,
                                        const gchar   *scheme,
                                        XfconfChannel *channel,
                                        guint          output);

void        xfce_randr_save_all        (XfceRandr     *randr,
                                        const gchar   *scheme,
                                        XfconfChannel *channel);

void        xfce_randr_apply           (XfceRandr     *randr,
                                        const gchar   *scheme,
                                        XfconfChannel *channel);

void        xfce_randr_load            (XfceRandr     *randr,
                                        const gchar   *scheme,
                                        XfconfChannel *channel);

gchar      *xfce_randr_friendly_name   (XfceRandr     *randr,
                                        RROutput       output,
                                        const gchar   *name);

XfceRRMode *xfce_randr_find_mode_by_id (XfceRandr     *randr,
                                        guint          output,
                                        RRMode         id);

RRMode      xfce_randr_preferred_mode  (XfceRandr     *randr,
                                        guint          output);

#endif /* !__XFCE_RANDR_H__ */
