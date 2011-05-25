/*-
 * Copyright (c) 2008 Jannis Pohlmann <jannis@xfce.org>
 * Copyright (c) 2005 Benedikt Meurer <benny@xfce.org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef __XFCE_TEXT_RENDERER_H__
#define __XFCE_TEXT_RENDERER_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS;

typedef struct _XfceTextRendererClass XfceTextRendererClass;
typedef struct _XfceTextRenderer      XfceTextRenderer;

#define XFCE_TYPE_TEXT_RENDERER            (xfce_text_renderer_get_type ())
#define XFCE_TEXT_RENDERER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), XFCE_TYPE_TEXT_RENDERER, XfceTextRenderer))
#define XFCE_TEXT_RENDERER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), XFCE_TYPE_TEXT_RENDERER, XfceTextRendererClass))
#define XFCE_IS_TEXT_RENDERER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), XFCE_TYPE_TEXT_RENDERER))
#define XFCE_IS_TEXT_RENDERER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), XFCE_TYPE_TEXT_RENDERER))
#define XFCE_TEXT_RENDERER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), XFCE_TYPE_TEXT_RENDERER, XfceTextRendererClass))

GType            xfce_text_renderer_get_type (void) G_GNUC_CONST;

GtkCellRenderer *xfce_text_renderer_new      (void) G_GNUC_MALLOC;

G_END_DECLS;

#endif /* !__XFCE_TEXT_RENDERER_H__ */
