/*
 * Copyright (C) 2012 Nick Schermer <nick@xfce.org>
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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef __XFCE_MIME_CHOOSER_H__
#define __XFCE_MIME_CHOOSER_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define XFCE_TYPE_MIME_CHOOSER (xfce_mime_chooser_get_type ())
G_DECLARE_FINAL_TYPE (XfceMimeChooser, xfce_mime_chooser, XFCE, MIME_CHOOSER, GtkDialog)

void
xfce_mime_chooser_set_mime_type (XfceMimeChooser *chooser,
                                 const gchar *mime_type,
                                 gint selected_mime_type_count);

GAppInfo *
xfce_mime_chooser_get_app_info (XfceMimeChooser *chooser);

G_END_DECLS

#endif /* !__XFCE_MIME_CHOOSER_H__ */
