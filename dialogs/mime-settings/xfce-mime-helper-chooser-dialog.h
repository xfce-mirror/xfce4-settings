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

#ifndef __XFCE_MIME_HELPER_CHOOSER_DIALOG_H__
#define __XFCE_MIME_HELPER_CHOOSER_DIALOG_H__

#include <xfce-mime-helper-chooser.h>

G_BEGIN_DECLS

typedef struct _XfceMimeHelperChooserDialogClass XfceMimeHelperChooserDialogClass;
typedef struct _XfceMimeHelperChooserDialog      XfceMimeHelperChooserDialog;

#define XFCE_MIME_TYPE_HELPER_CHOOSER_DIALOG            (xfce_mime_helper_chooser_dialog_get_type ())
#define XFCE_MIME_HELPER_CHOOSER_DIALOG(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), XFCE_MIME_TYPE_HELPER_CHOOSER_DIALOG, XfceMimeHelperChooserDialog))
#define XFCE_MIME_HELPER_CHOOSER_DIALOG_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), XFCE_MIME_TYPE_HELPER_CHOOSER_DIALOG, XfceMimeHelperChooserDialogClass))
#define XFCE_MIME_IS_HELPER_CHOOSER_DIALOG(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), XFCE_MIME_TYPE_HELPER_CHOOSER_DIALOG))
#define XFCE_MIME_IS_HELPER_CHOOSER_DIALOG_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), XFCE_MIME_TYPE_HELPER_CHOOSER_DIALOG))
#define XFCE_MIME_HELPER_CHOOSER_DIALOG_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), XFCE_MIME_TYPE_HELPER_CHOOSER_DIALOG, XfceMimeHelperChooserDialogClass))

GType      xfce_mime_helper_chooser_dialog_get_type (void) G_GNUC_CONST;

GtkWidget *xfce_mime_helper_chooser_dialog_new      (void) G_GNUC_MALLOC;

GtkWidget *xfce_mime_helper_chooser_dialog_get_plug_child (XfceMimeHelperChooserDialog *dialog);

G_END_DECLS

#endif /* !__XFCE_MIME_HELPER_CHOOSER_DIALOG_H__ */
