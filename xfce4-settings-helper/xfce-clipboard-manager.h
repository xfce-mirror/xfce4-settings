/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2007 William Jon McCann <mccann@jhu.edu>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 */

#ifndef __XFCE_CLIPBOARD_MANAGER_H
#define __XFCE_CLIPBOARD_MANAGER_H

#include <glib-object.h>

G_BEGIN_DECLS

#define XFCE_TYPE_CLIPBOARD_MANAGER         (xfce_clipboard_manager_get_type ())
#define XFCE_CLIPBOARD_MANAGER(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), XFCE_TYPE_CLIPBOARD_MANAGER, XfceClipboardManager))
#define XFCE_CLIPBOARD_MANAGER_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), XFCE_TYPE_CLIPBOARD_MANAGER, XfceClipboardManagerClass))
#define XFCE_IS_CLIPBOARD_MANAGER(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), XFCE_TYPE_CLIPBOARD_MANAGER))
#define XFCE_IS_CLIPBOARD_MANAGER_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), XFCE_TYPE_CLIPBOARD_MANAGER))
#define XFCE_CLIPBOARD_MANAGER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), XFCE_TYPE_CLIPBOARD_MANAGER, XfceClipboardManagerClass))

typedef struct XfceClipboardManagerPrivate XfceClipboardManagerPrivate;

typedef struct
{
  GObject                      parent;
  XfceClipboardManagerPrivate *priv;
} XfceClipboardManager;

typedef struct
{
  GObjectClass   parent_class;
} XfceClipboardManagerClass;

GType                 xfce_clipboard_manager_get_type            (void);

XfceClipboardManager *xfce_clipboard_manager_new                 (void);
gboolean              xfce_clipboard_manager_start               (XfceClipboardManager  *manager,
                                                                  GError               **error);
void                  xfce_clipboard_manager_stop                (XfceClipboardManager  *manager);

G_END_DECLS

#endif /* __XFCE_CLIPBOARD_MANAGER_H */
