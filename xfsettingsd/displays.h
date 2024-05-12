/*
 *  Copyright (c) 2008 Nick Schermer <nick@xfce.org>
 *  Copyright (C) 2023 Gaël Bonithon <gael@xfce.org>
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

#ifndef __DISPLAYS_H__
#define __DISPLAYS_H__

#include <glib-object.h>
#include <xfconf/xfconf.h>

/* Xfconf properties */
#define APPLY_SCHEME_PROP "/Schemes/Apply"
#define DEFAULT_SCHEME_NAME "Default"
#define ACTIVE_PROFILE "/ActiveProfile"
#define AUTO_ENABLE_PROFILES "/AutoEnableProfiles"
#define OUTPUT_FMT "/%s/%s"
#define PRIMARY_PROP OUTPUT_FMT "/Primary"
#define ACTIVE_PROP OUTPUT_FMT "/Active"
#define ROTATION_PROP OUTPUT_FMT "/Rotation"
#define REFLECTION_PROP OUTPUT_FMT "/Reflection"
#define RESOLUTION_PROP OUTPUT_FMT "/Resolution"
#define SCALE_PROP OUTPUT_FMT "/Scale"
#define SCALEX_PROP OUTPUT_FMT "/Scale/X"
#define RRATE_PROP OUTPUT_FMT "/RefreshRate"
#define POSX_PROP OUTPUT_FMT "/Position/X"
#define POSY_PROP OUTPUT_FMT "/Position/Y"
#define NOTIFY_PROP "/Notify"

/* some messages belonging to parts of code common to X11/Wayland implementations
 * that are not easy to share in the parent class */
#define WARNING_MESSAGE_UNKNOWN_MODE "Unknown mode '%s @ %.1f' for output %s, aborting."
#define WARNING_MESSAGE_ALL_DISABLED "Stored Xfconf properties disable all outputs, aborting."
#define DEBUG_MESSAGE_TOGGLING_INTERNAL "Toggling internal output %s."
#define DEBUG_MESSAGE_DISABLING_INTERNAL "%s will be disabled."
#define DEBUG_MESSAGE_ENABLING_INTERNAL "%s will be re-enabled."
#define DEBUG_MESSAGE_TOTAL_ACTIVE "Total %d active output(s)."
#define DEBUG_MESSAGE_DIFF_N_OUTPUTS "Noutput: before = %d, after = %d."
#define DEBUG_MESSAGE_ALL_DISABLED "No active output anymore! Attempting to re-enable the internal output."
#define DEBUG_MESSAGE_NEW_OUTPUT "New output connected: %s"

G_BEGIN_DECLS

#define XFCE_TYPE_DISPLAYS_HELPER (xfce_displays_helper_get_type ())
G_DECLARE_DERIVABLE_TYPE (XfceDisplaysHelper, xfce_displays_helper, XFCE, DISPLAYS_HELPER, GObject)

struct _XfceDisplaysHelperClass
{
    GObjectClass __parent__;

    GPtrArray *(*get_outputs) (XfceDisplaysHelper *helper);
    void (*toggle_internal) (gpointer *power,
                             gboolean lid_is_closed,
                             XfceDisplaysHelper *helper);
    gchar **(*get_display_infos) (XfceDisplaysHelper *helper);
    void (*channel_apply) (XfceDisplaysHelper *helper,
                           const gchar *scheme);
};

GObject *
xfce_displays_helper_new (void);
gchar *
xfce_displays_helper_get_matching_profile (XfceDisplaysHelper *helper);
XfconfChannel *
xfce_displays_helper_get_channel (XfceDisplaysHelper *helper);

G_END_DECLS

#endif /* !__DISPLAYS_H__ */
