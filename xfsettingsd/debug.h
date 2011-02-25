/*
 *  Copyright (c) 2011 Nick Schermer <nick@xfce.org>
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
 */

#ifndef __DEBUG_H__
#define __DEBUG_H__

typedef enum
{
   XFSD_DEBUG_YES                = 1 << 0,

   /* filter levels */
   XFSD_DEBUG_XSETTINGS          = 1 << 1,
   XFSD_DEBUG_FONTCONFIG         = 1 << 2,
   XFSD_DEBUG_KEYBOARD_LAYOUT    = 1 << 3,
   XFSD_DEBUG_KEYBOARDS          = 1 << 4,
   XFSD_DEBUG_KEYBOARD_SHORTCUTS = 1 << 5,
   XFSD_DEBUG_WORKSPACES         = 1 << 6,
}
XfsdDebugDomain;

void xfsettings_dbg          (XfsdDebugDomain  domain,
                              const gchar     *message,
                              ...) G_GNUC_PRINTF (2, 3);

void xfsettings_dbg_filtered (XfsdDebugDomain  domain,
                              const gchar     *message,
                              ...) G_GNUC_PRINTF (2, 3);

#endif /* !__DEBUG_H__ */
