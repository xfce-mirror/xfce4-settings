/*
 *  Copyright (c) 2019 Simon Steinbeiß <simon@xfce.org>
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
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef __DISPLAY_PROFILES_H__
#define __DISPLAY_PROFILES_H__

#include <glib.h>
#include <xfconf/xfconf.h>

G_BEGIN_DECLS

enum
{
    ACTION_ON_NEW_OUTPUT_DO_NOTHING,
    ACTION_ON_NEW_OUTPUT_SHOW_DIALOG,
    ACTION_ON_NEW_OUTPUT_MIRROR,
    ACTION_ON_NEW_OUTPUT_EXTEND,
};

enum
{
    AUTO_ENABLE_PROFILES_NEVER,
    AUTO_ENABLE_PROFILES_ON_CONNECT,
    AUTO_ENABLE_PROFILES_ON_DISCONNECT,
    AUTO_ENABLE_PROFILES_ALWAYS,
};

#define ACTION_ON_NEW_OUTPUT_DEFAULT ACTION_ON_NEW_OUTPUT_SHOW_DIALOG
#define AUTO_ENABLE_PROFILES_DEFAULT AUTO_ENABLE_PROFILES_ALWAYS

gboolean
display_settings_profile_name_exists (XfconfChannel *channel,
                                      const gchar *new_profile_name);
GList *
display_settings_get_profiles (gchar **display_infos,
                               XfconfChannel *channel,
                               gboolean matching_only);
gboolean
display_settings_profile_matches (const gchar *profile,
                                  gchar **display_infos,
                                  XfconfChannel *channel);

G_END_DECLS

#endif /* !__DISPLAY_PROFILES_H__ */
