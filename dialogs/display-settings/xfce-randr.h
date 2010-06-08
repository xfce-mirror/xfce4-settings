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

#ifndef __XFCE_RANDR_H__
#define __XFCE_RANDR_H__

#define XFCE_RANDR_MODE(randr)            (randr->mode[randr->active_output])
#define XFCE_RANDR_PREFERRED_MODE(randr)  (randr->preferred_mode[randr->active_output])
#define XFCE_RANDR_ROTATION(randr)        (randr->rotation[randr->active_output])
#define XFCE_RANDR_ROTATIONS(randr)       (randr->rotations[randr->active_output])
#define XFCE_RANDR_OUTPUT_INFO(randr)     (randr->output_info[randr->active_output])
#define XFCE_RANDR_POSITION_OPTION(randr) (randr->position[randr->active_output].option)
#define XFCE_RANDR_POSITION_OUTPUT(randr) (randr->position[randr->active_output].output)

/* check for randr 1.2 or better */
#if RANDR_MAJOR > 1 || (RANDR_MAJOR == 1 && RANDR_MINOR >= 2)
#define HAS_RANDR_ONE_POINT_TWO
/* check for randr 1.3 or better */
#if RANDR_MAJOR > 1 || (RANDR_MAJOR == 1 && RANDR_MINOR >= 3)
#define HAS_RANDR_ONE_POINT_THREE
#endif
#else
#undef HAS_RANDR_ONE_POINT_TWO
#undef HAS_RANDR_ONE_POINT_THREE
#endif

#ifdef HAS_RANDR_ONE_POINT_TWO
typedef struct _XfceRandr                XfceRandr;
typedef struct _XfceOutputPosition       XfceOutputPosition;
typedef enum   _XfceDisplayLayout        XfceDisplayLayout;
typedef enum   _XfceOutputPositionOption XfceOutputPositionOption;
typedef enum   _XfceOutputStatus         XfceOutputStatus;

enum _XfceDisplayLayout
{
    XFCE_DISPLAY_LAYOUT_SINGLE,
    XFCE_DISPLAY_LAYOUT_CLONE,
    XFCE_DISPLAY_LAYOUT_EXTEND
};

enum _XfceOutputPositionOption
{
    XFCE_OUTPUT_POSITION_LEFT_OF,
    XFCE_OUTPUT_POSITION_RIGHT_OF,
    XFCE_OUTPUT_POSITION_ABOVE,
    XFCE_OUTPUT_POSITION_BELOW,
    XFCE_OUTPUT_POSITION_SAME_AS
};

enum _XfceOutputStatus
{
    XFCE_OUTPUT_STATUS_NONE,
    XFCE_OUTPUT_STATUS_PRIMARY,
    XFCE_OUTPUT_STATUS_SECONDARY
};

struct _XfceOutputPosition
{
    /* option... */
    XfceOutputPositionOption option;

    /* ... relative to the position of */
    gint                     output;
};

struct _XfceRandr
{
    /* xrandr 1.3 capable */
    gint                 has_1_3;

    /* display for this randr config */
    GdkDisplay          *display;
    
    /* screen resource for this display */
    XRRScreenResources  *resources;

    /* the active selected layout */
    gint                 active_output;

    /* cache for the output info */
    XRROutputInfo      **output_info;

    /* selected display layout */
    XfceDisplayLayout    layout;

    /* selected settings for all outputs */
    RRMode              *mode;
    RRMode              *preferred_mode;
    Rotation            *rotation;
    Rotation            *rotations;
    XfceOutputPosition  *position;
    XfceOutputStatus    *status;
};



XfceRandr   *xfce_randr_new           (GdkDisplay    *display,
                                       GError       **error);

void         xfce_randr_free          (XfceRandr     *randr);

void         xfce_randr_reload        (XfceRandr     *randr);

void         xfce_randr_save          (XfceRandr     *randr,
                                       const gchar   *scheme,
                                       XfconfChannel *channel);

void         xfce_randr_load          (XfceRandr     *randr,
                                       const gchar   *scheme,
                                       XfconfChannel *channel);

const gchar *xfce_randr_friendly_name (XfceRandr     *randr,
                                       RROutput       output,
                                       const gchar   *name);

#endif /* !HAS_RANDR_ONE_POINT_TWO */

#endif /* !__XFCE_RANDR_H__ */
