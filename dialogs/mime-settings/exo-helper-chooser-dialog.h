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

#ifndef __EXO_HELPER_CHOOSER_DIALOG_H__
#define __EXO_HELPER_CHOOSER_DIALOG_H__

#include <exo-helper/exo-helper-chooser.h>

G_BEGIN_DECLS

typedef struct _ExoHelperChooserDialogClass ExoHelperChooserDialogClass;
typedef struct _ExoHelperChooserDialog      ExoHelperChooserDialog;

#define EXO_TYPE_HELPER_CHOOSER_DIALOG            (exo_helper_chooser_dialog_get_type ())
#define EXO_HELPER_CHOOSER_DIALOG(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), EXO_TYPE_HELPER_CHOOSER_DIALOG, ExoHelperChooserDialog))
#define EXO_HELPER_CHOOSER_DIALOG_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), EXO_TYPE_HELPER_CHOOSER_DIALOG, ExoHelperChooserDialogClass))
#define EXO_IS_HELPER_CHOOSER_DIALOG(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EXO_TYPE_HELPER_CHOOSER_DIALOG))
#define EXO_IS_HELPER_CHOOSER_DIALOG_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), EXO_TYPE_HELPER_CHOOSER_DIALOG))
#define EXO_HELPER_CHOOSER_DIALOG_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), EXO_TYPE_HELPER_CHOOSER_DIALOG, ExoHelperChooserDialogClass))

GType      exo_helper_chooser_dialog_get_type (void) G_GNUC_CONST;

GtkWidget *exo_helper_chooser_dialog_new      (void) G_GNUC_MALLOC;

GtkWidget *exo_helper_chooser_dialog_get_plug_child (ExoHelperChooserDialog *dialog);

G_END_DECLS

#endif /* !__EXO_HELPER_CHOOSER_DIALOG_H__ */
