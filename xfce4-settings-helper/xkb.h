/* $Id$ */
/*
 *  Copyright (c) 2008 Stephan Arts <stephan@xfce.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 *  XKB Extension code taken from the original mcs-keyboard-plugin written
 *  by Olivier Fourdan.
 */

#ifndef __XKB_H__
#define __XKB_H__

typedef struct _XfceXkbHelperClass XfceXkbHelperClass;
typedef struct _XfceXkbHelper      XfceXkbHelper;

#define XFCE_TYPE_XKB_HELPER            (xfce_xkb_helper_get_type ())
#define XFCE_XKB_HELPER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), XFCE_TYPE_XKB_HELPER, XfceXkbHelper))
#define XFCE_XKB_HELPER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), XFCE_TYPE_XKB_HELPER, XfceXkbHelperClass))
#define XFCE_IS_XKB_HELPER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), XFCE_TYPE_XKB_HELPER))
#define XFCE_IS_XKB_HELPER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), XFCE_TYPE_XKB_HELPER))
#define XFCE_XKB_HELPER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), XFCE_TYPE_XKB_HELPER, XfceXkbHelperClass))

GType xfce_xkb_helper_get_type (void) G_GNUC_CONST;

#endif /* !__XKB_H__ */
