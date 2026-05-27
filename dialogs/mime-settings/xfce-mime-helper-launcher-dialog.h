/*-
 * Copyright (c) 2003-2006 Benedikt Meurer <benny@xfce.org>.
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
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef __XFCE_MIME_HELPER_LAUNCHER_DIALOG_H__
#define __XFCE_MIME_HELPER_LAUNCHER_DIALOG_H__

#include "xfce-mime-helper.h"

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define XFCE_MIME_TYPE_HELPER_LAUNCHER_DIALOG (xfce_mime_helper_launcher_dialog_get_type ())
G_DECLARE_FINAL_TYPE (XfceMimeHelperLauncherDialog, xfce_mime_helper_launcher_dialog, XFCE_MIME, HELPER_LAUNCHER_DIALOG, GtkDialog)

GtkWidget *
xfce_mime_helper_launcher_dialog_new (XfceMimeHelperCategory category) G_GNUC_MALLOC;

XfceMimeHelperCategory
xfce_mime_helper_launcher_dialog_get_category (XfceMimeHelperLauncherDialog *launcher_dialog);
void
xfce_mime_helper_launcher_dialog_set_category (XfceMimeHelperLauncherDialog *launcher_dialog,
                                               XfceMimeHelperCategory category);

G_END_DECLS

#endif /* !__XFCE_MIME_HELPER_LAUNCHER_DIALOG_H__ */
