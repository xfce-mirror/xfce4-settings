/*
 *  xfce4-settings-editor
 *
 *  Copyright (c) 2012      Nick Schermer <nick@xfce.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License ONLY.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef __XFCE_SETTINGS_CELL_RENDERER_H__
#define __XFCE_SETTINGS_CELL_RENDERER_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define XFCE_TYPE_SETTINGS_CELL_RENDERER (xfce_settings_cell_renderer_get_type ())
G_DECLARE_FINAL_TYPE (XfceSettingsCellRenderer, xfce_settings_cell_renderer, XFCE, SETTINGS_CELL_RENDERER, GtkCellRenderer)

GtkCellRenderer *
xfce_settings_cell_renderer_new (void);

GType
xfce_settings_array_type (void);

G_END_DECLS

#endif /* __XFCE_SETTINGS_CELL_RENDERER_H__ */
