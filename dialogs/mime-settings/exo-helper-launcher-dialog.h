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

#ifndef __EXO_HELPER_LAUNCHER_DIALOG_H__
#define __EXO_HELPER_LAUNCHER_DIALOG_H__

#include <exo-helper/exo-helper.h>

G_BEGIN_DECLS

typedef struct _ExoHelperLauncherDialogClass ExoHelperLauncherDialogClass;
typedef struct _ExoHelperLauncherDialog      ExoHelperLauncherDialog;

#define EXO_TYPE_HELPER_LAUNCHER_DIALOG             (exo_helper_launcher_dialog_get_type ())
#define EXO_HELPER_LAUNCHER_DIALOG(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), EXO_TYPE_HELPER_LAUNCHER_DIALOG, ExoHelperLauncherDialog))
#define EXO_HELPER_LAUNCHER_DIALOG_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), EXO_TYPE_HELPER_LAUNCHER_DIALOG, ExoHelperLauncherDialogClass))
#define EXO_IS_HELPER_LAUNCHER_DIALOG(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EXO_TYPE_HELPER_LAUNCHER_DIALOG))
#define EXO_IS_HELPER_LAUNCHER_DIALOG_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), EXO_TYPE_HELPER_LAUNCHER_DIALOG))
#define EXO_HELPER_LAUNCHER_DIALOG_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), EXOT_TYPE_HELPER_LAUNCHER_DIALOg, ExoHelperLauncherDialogClass))

GType             exo_helper_launcher_dialog_get_type     (void) G_GNUC_CONST;

GtkWidget        *exo_helper_launcher_dialog_new          (ExoHelperCategory              category) G_GNUC_MALLOC;

ExoHelperCategory exo_helper_launcher_dialog_get_category (const ExoHelperLauncherDialog *launcher_dialog);
void              exo_helper_launcher_dialog_set_category (ExoHelperLauncherDialog       *launcher_dialog,
                                                           ExoHelperCategory              category);

G_END_DECLS

#endif /* !__EXO_HELPER_LAUNCHER_DIALOG_H__ */
