/* $Id$ */
/*
 *  xfce4-settings-manager
 *
 *  Copyright (c) 2008 Brian Tarricone <bjt23@cornell.edu>
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

#ifndef __XFCE_SETTIGNS_MANAGER_DIALOG_H__
#define __XFCE_SETTINGS_MANAGER_DIALOG_H__

#include <gtk/gtk.h>

#define XFCE_TYPE_SETTINGS_MANAGER_DIALOG     (xfce_settings_manager_dialog_get_type())
#define XFCE_SETTINGS_MANAGER_DIALOG(obj)     (G_TYPE_CHECK_INSTANCE_CAST((obj), XFCE_TYPE_SETTINGS_MANAGER_DIALOG, XfceSettingsManagerDialog))
#define XFCE_IS_SETTINGS_MANAGER_DIALOG(obj)  (G_TYPE_CHECK_INSTANCE_TYPE((obj), XFCE_TYPE_SETTINGS_MANAGER_DIALOG))

G_BEGIN_DECLS

typedef struct _XfceSettingsManagerDialog  XfceSettingsManagerDialog;

GType xfce_settings_manager_dialog_get_type() G_GNUC_CONST;

GtkWidget *xfce_settings_manager_dialog_new();

G_END_DECLS

#endif  /* __XFCE_SETTINGS_MANAGER_DIALOG_H__ */
