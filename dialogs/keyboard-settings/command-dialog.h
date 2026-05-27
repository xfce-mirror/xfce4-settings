/* vi:set sw=2 sts=2 ts=2 et ai: */
/*-
 * Copyright (c) 2008 Jannis Pohlmann <jannis@xfce.org>.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef __COMMAND_DIALOG_H__
#define __COMMAND_DIALOG_H__

#include <gtk/gtk.h>
#include <libxfce4ui/libxfce4ui.h>

G_BEGIN_DECLS

#define COMMAND_TYPE_DIALOG (command_dialog_get_type ())
#ifndef glib_autoptr_clear_XfceTitledDialog
G_DEFINE_AUTOPTR_CLEANUP_FUNC (XfceTitledDialog, g_object_unref)
#endif
G_DECLARE_FINAL_TYPE (CommandDialog, command_dialog, COMMAND, DIALOG, XfceTitledDialog)

GtkWidget *
command_dialog_new (const gchar *shortcut,
                    const gchar *action,
                    gboolean snotify);
const char *
command_dialog_get_command (CommandDialog *dialog);
gboolean
command_dialog_get_snotify (CommandDialog *dialog);
gint
command_dialog_run (CommandDialog *dialog,
                    GtkWidget *parent);

G_END_DECLS

#endif /* !__COMMAND_DIALOG_H__ */
