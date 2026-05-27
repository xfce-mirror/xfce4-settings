/*
 * Copyright (c) 2008 Stephan Arts <stephan@xfce.org>
 * Copyright (c) 2011 Nick Schermer <nick@xfce.org>
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

#ifndef __XSETTINGS_H__
#define __XSETTINGS_H__

#include <X11/Xlib.h>
#include <gdk/gdk.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define XFCE_TYPE_XSETTINGS_HELPER (xfce_xsettings_helper_get_type ())
G_DECLARE_FINAL_TYPE (XfceXSettingsHelper, xfce_xsettings_helper, XFCE, XSETTINGS_HELPER, GObject)

gboolean
xfce_xsettings_helper_register (XfceXSettingsHelper *helper,
                                GdkDisplay *gdkdisplay,
                                gboolean force_replace);

Time
xfce_xsettings_get_server_time (Display *display,
                                Window window);

G_END_DECLS

#endif /* !__XSETTINGS_H__ */
