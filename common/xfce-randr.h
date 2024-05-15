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

#ifndef __XFCE_RANDR_H__
#define __XFCE_RANDR_H__

#include <X11/extensions/Xrandr.h>
#include <gdk/gdk.h>
#include <xfconf/xfconf.h>

#define XFCE_RANDR_ROTATIONS_MASK (RR_Rotate_0 | RR_Rotate_90 | RR_Rotate_180 | RR_Rotate_270)
#define XFCE_RANDR_REFLECTIONS_MASK (RR_Reflect_X | RR_Reflect_Y)

G_BEGIN_DECLS

typedef struct _XfceRandr XfceRandr;
typedef struct _XfceRandrPrivate XfceRandrPrivate;
typedef struct _XfceRRMode XfceRRMode;
typedef enum _XfceOutputStatus XfceOutputStatus;
typedef struct _XfceOutputPosition XfceOutputPosition;

enum _XfceOutputStatus
{
    XFCE_OUTPUT_STATUS_PRIMARY,
    XFCE_OUTPUT_STATUS_SECONDARY
};

struct _XfceRRMode
{
    RRMode id;
    guint width;
    guint height;
    gdouble rate;
};

struct _XfceOutputPosition
{
    gint x;
    gint y;
};

struct _XfceRandr
{
    /* number of connected outputs */
    guint noutput;

    /* selected settings for all connected outputs */
    RRMode *mode;
    gdouble *scalex;
    gdouble *scaley;
    Rotation *rotation;
    Rotation *rotations;
    XfceOutputPosition *position;
    XfceOutputStatus *status;
    gboolean *mirrored;
    gchar **friendly_name;

    /* implementation details */
    XfceRandrPrivate *priv;
};

XfceRandr *
xfce_randr_new (GdkDisplay *display,
                GError **error);

void
xfce_randr_free (XfceRandr *randr);

void
xfce_randr_reload (XfceRandr *randr);

void
xfce_randr_save_output (XfceRandr *randr,
                        const gchar *scheme,
                        XfconfChannel *channel,
                        guint output);

void
xfce_randr_load (XfceRandr *randr,
                 const gchar *scheme,
                 XfconfChannel *channel);

guint8 *
xfce_randr_read_edid_data (Display *xdisplay,
                           RROutput output);

const XfceRRMode *
xfce_randr_find_mode_by_id (XfceRandr *randr,
                            guint output,
                            RRMode id);

RRMode
xfce_randr_preferred_mode (XfceRandr *randr,
                           guint output);

RRMode *
xfce_randr_clonable_modes (XfceRandr *randr);

gchar *
xfce_randr_get_edid (XfceRandr *randr,
                     guint noutput);

gchar *
xfce_randr_get_output_info_name (XfceRandr *randr,
                                 guint noutput);

const XfceRRMode *
xfce_randr_get_modes (XfceRandr *randr,
                      guint output,
                      gint *nmode);

gboolean
xfce_randr_get_positions (XfceRandr *randr,
                          guint output,
                          gint *x,
                          gint *y);

gchar **
xfce_randr_get_display_infos (XfceRandr *randr);

guint
xfce_randr_mode_width (XfceRandr *randr,
                       guint output,
                       const XfceRRMode *mode);

guint
xfce_randr_mode_height (XfceRandr *randr,
                        guint output,
                        const XfceRRMode *mode);

G_END_DECLS

#endif /* !__XFCE_RANDR_H__ */
