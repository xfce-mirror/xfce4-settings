/*
 *  xfce4-settings-editor
 *
 *  Copyright (c) 2008      Brian Tarricone <bjt23@cornell.edu>
 *  Copyright (c) 2008      Stephan Arts <stephan@xfce.org>
 *  Copyright (c) 2009-2010 Jérôme Guelfucci <jeromeg@xfce.org>
 *  Copyright (c) 2012      Nick Schermer <nick@xfce.org>
 *  Copyright (c) 2015		Ali Abdallah <ali@aliov.org>
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

#ifndef __XFCE_SETTINGS_EDITOR_BOX_H__
#define __XFCE_SETTINGS_EDITOR_BOX_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define XFCE_TYPE_SETTINGS_EDITOR_BOX (xfce_settings_editor_box_get_type ())
G_DECLARE_FINAL_TYPE (XfceSettingsEditorBox, xfce_settings_editor_box, XFCE, SETTINGS_EDITOR_BOX, GtkBox)

GtkWidget *
xfce_settings_editor_box_new (gint paned_pos);

G_END_DECLS

#endif /* __XFCE_SETTINGS_EDITOR_BOX_H__ */
