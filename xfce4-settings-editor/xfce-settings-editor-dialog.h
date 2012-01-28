/*
 *  xfce4-settings-editor
 *
 *  Copyright (c) 2008      Brian Tarricone <bjt23@cornell.edu>
 *  Copyright (c) 2008      Stephan Arts <stephan@xfce.org>
 *  Copyright (c) 2009-2010 Jérôme Guelfucci <jeromeg@xfce.org>
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

#ifndef __XFCE_SETTINGS_EDITOR_DIALOG_H__
#define __XFCE_SETTINGS_EDITOR_DIALOG_H__

#include <gtk/gtk.h>

#define XFCE_TYPE_SETTINGS_EDITOR_DIALOG            (xfce_settings_editor_dialog_get_type ())
#define XFCE_SETTINGS_EDITOR_DIALOG(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), XFCE_TYPE_SETTINGS_EDITOR_DIALOG, XfceSettingsEditorDialog))
#define XFCE_SETTINGS_EDITOR_DIALOG_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), XFCE_TYPE_SETTINGS_EDITOR_DIALOG, XfceSettingsEditorDialogClass))
#define XFCE_IS_SETTINGS_EDITOR_DIALOG(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), XFCE_TYPE_SETTINGS_EDITOR_DIALOG))
#define XFCE_IS_SETTINGS_EDITOR_DIALOG_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), XFCE_TYPE_SETTINGS_EDITOR_DIALOG))
#define XFCE_SETTINGS_EDITOR_DIALOG_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), XFCE_TYPE_SETTINGS_EDITOR_DIALOG, XfceSettingsEditorDialogClass))

G_BEGIN_DECLS

typedef struct _XfceSettingsEditorDialog      XfceSettingsEditorDialog;
typedef struct _XfceSettingsEditorDialogClass XfceSettingsEditorDialogClass;

GType      xfce_settings_editor_dialog_get_type    (void) G_GNUC_CONST;

GtkWidget *xfce_settings_editor_dialog_new         (void);

G_END_DECLS

#endif  /* __XFCE_SETTINGS_EDITOR_DIALOG_H__ */
