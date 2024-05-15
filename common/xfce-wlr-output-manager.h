/*
 *  Copyright (c) 2023 Gaël Bonithon <gael@xfce.org>
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

#ifndef __XFCE_WLR_OUTPUT_MANAGER_H__
#define __XFCE_WLR_OUTPUT_MANAGER_H__

#include "protocols/wlr-output-management-unstable-v1-client.h"

#include <glib-object.h>

G_BEGIN_DECLS

#define XFCE_TYPE_WLR_OUTPUT_MANAGER (xfce_wlr_output_manager_get_type ())
G_DECLARE_FINAL_TYPE (XfceWlrOutputManager, xfce_wlr_output_manager, XFCE, WLR_OUTPUT_MANAGER, GObject)

typedef enum _XfceWlrTransform
{
    XFCE_WLR_TRANSFORM_NORMAL,
    XFCE_WLR_TRANSFORM_90,
    XFCE_WLR_TRANSFORM_180,
    XFCE_WLR_TRANSFORM_270,
    XFCE_WLR_TRANSFORM_FLIPPED,
    XFCE_WLR_TRANSFORM_FLIPPED_90,
    XFCE_WLR_TRANSFORM_FLIPPED_180,
    XFCE_WLR_TRANSFORM_FLIPPED_270,
} XfceWlrTransform;

typedef struct _XfceWlrOutput
{
    XfceWlrOutputManager *manager;
    struct zwlr_output_head_v1 *wl_head;
    gboolean new;

    /* read only */
    gchar *name;
    gchar *description;
    int32_t width, height;
    GList *modes;
    gchar *manufacturer;
    gchar *model;
    gchar *serial_number;
    gchar *edid;

    /* writable */
    int32_t enabled;
    struct zwlr_output_mode_v1 *wl_mode;
    int32_t x, y;
    int32_t transform;
    wl_fixed_t scale;
    uint32_t adaptive_sync;
} XfceWlrOutput;

typedef struct _XfceWlrMode
{
    XfceWlrOutput *output;
    struct zwlr_output_mode_v1 *wl_mode;
    int32_t width, height;
    int32_t refresh;
    gboolean preferred;
} XfceWlrMode;

typedef void (*XfceWlrOutputListener) (XfceWlrOutputManager *manager,
                                       struct zwlr_output_manager_v1 *wl_manager,
                                       uint32_t serial);

XfceWlrOutputManager *
xfce_wlr_output_manager_new (XfceWlrOutputListener listener,
                             gpointer listener_data);
gpointer
xfce_wlr_output_manager_get_listener_data (XfceWlrOutputManager *manager);
struct zwlr_output_manager_v1 *
xfce_wlr_output_manager_get_wl_manager (XfceWlrOutputManager *manager);
GPtrArray *
xfce_wlr_output_manager_get_outputs (XfceWlrOutputManager *manager);
gchar **
xfce_wlr_output_manager_get_display_infos (XfceWlrOutputManager *manager);

G_END_DECLS

#endif /* !__XFCE_WLR_OUTPUT_MANAGER_H__ */
