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

#ifndef __KEYBOARDS_H__
#define __KEYBOARDS_H__

typedef struct _XfceKeyboardsHelperClass XfceKeyboardsHelperClass;
typedef struct _XfceKeyboardsHelper      XfceKeyboardsHelper;

#define XFCE_TYPE_KEYBOARDS_HELPER            (xfce_keyboards_helper_get_type ())
#define XFCE_KEYBOARDS_HELPER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), XFCE_TYPE_KEYBOARDS_HELPER, XfceKeyboardsHelper))
#define XFCE_KEYBOARDS_HELPER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), XFCE_TYPE_KEYBOARDS_HELPER, XfceKeyboardsHelperClass))
#define XFCE_IS_KEYBOARDS_HELPER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), XFCE_TYPE_KEYBOARDS_HELPER))
#define XFCE_IS_KEYBOARDS_HELPER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), XFCE_TYPE_KEYBOARDS_HELPER))
#define XFCE_KEYBOARDS_HELPER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), XFCE_TYPE_KEYBOARDS_HELPER, XfceKeyboardsHelperClass))

GType xfce_keyboards_helper_get_type (void) G_GNUC_CONST;

#endif /* !__KEYBOARDS_H__ */
