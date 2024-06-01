/*
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

#ifndef __DISPLAY_SETTINGS_H__
#define __DISPLAY_SETTINGS_H__

#include <glib-object.h>
#include <gtk/gtk.h>
#include <xfconf/xfconf.h>

#define ROTATION_MASK (ROTATION_FLAGS_0 | ROTATION_FLAGS_90 | ROTATION_FLAGS_180 | ROTATION_FLAGS_270)
#define REFLECTION_MASK (ROTATION_FLAGS_REFLECT_X | ROTATION_FLAGS_REFLECT_Y)

G_BEGIN_DECLS

#define XFCE_TYPE_DISPLAY_SETTINGS (xfce_display_settings_get_type ())
G_DECLARE_DERIVABLE_TYPE (XfceDisplaySettings, xfce_display_settings, XFCE, DISPLAY_SETTINGS, GObject)

enum
{
    COLUMN_OUTPUT_NAME,
    COLUMN_OUTPUT_ID,
    N_OUTPUT_COLUMNS
};

enum
{
    COLUMN_ICON,
    COLUMN_NAME,
    COLUMN_HASH,
    COLUMN_MARKUP,
    COLUMN_MATCHES,
    N_COLUMNS
};

typedef enum _MirroredState
{
    MIRRORED_STATE_NONE, // at least 2 outputs have different x/y
    MIRRORED_STATE_MIRRORED, // all outputs have same x/y, at least two of them have different resolutions
    MIRRORED_STATE_CLONED, // all outputs have same x/y and same resolution
} MirroredState;

typedef enum _RotationFlags
{
    ROTATION_FLAGS_0 = 0,
    ROTATION_FLAGS_90 = 1 << 0,
    ROTATION_FLAGS_180 = 1 << 1,
    ROTATION_FLAGS_270 = 1 << 2,
    ROTATION_FLAGS_REFLECT_X = 1 << 3,
    ROTATION_FLAGS_REFLECT_Y = 1 << 4,
    ROTATION_FLAGS_ALL = (1 << 5) - 1,
} RotationFlags;

typedef enum _ExtendedMode
{
    EXTENDED_MODE_NONE = -1,
    EXTENDED_MODE_RIGHT,
    EXTENDED_MODE_LEFT,
    EXTENDED_MODE_UP,
    EXTENDED_MODE_DOWN,
} ExtendedMode;

typedef struct _XfceMode
{
    guint id;
    guint width;
    guint height;
    gdouble rate;
} XfceMode;

typedef struct _XfceOutput
{
    /* Identifiers */
    guint id;
    const gchar *friendly_name;

    /* Status */
    gboolean active;

    /* Position */
    gint x;
    gint y;

    /* Dimensions */
    guint pref_width;
    guint pref_height;

    /* Transformation */
    RotationFlags rotation;
    gdouble scale;

    /* Modes */
    XfceMode *mode;
    XfceMode **modes;
    guint n_modes;

    /* User Data (e.g. GrabInfo) */
    gpointer user_data;
} XfceOutput;

struct _XfceDisplaySettingsClass
{
    GObjectClass __parent__;

    guint (*get_n_outputs) (XfceDisplaySettings *settings);
    guint (*get_n_active_outputs) (XfceDisplaySettings *settings);
    gchar **(*get_display_infos) (XfceDisplaySettings *settings);
    GdkMonitor *(*get_monitor) (XfceDisplaySettings *settings,
                                guint output_id);
    const gchar *(*get_friendly_name) (XfceDisplaySettings *settings,
                                       guint output_id);
    void (*get_geometry) (XfceDisplaySettings *settings,
                          guint output_id,
                          GdkRectangle *geometry);
    RotationFlags (*get_rotation) (XfceDisplaySettings *settings,
                                   guint output_id);
    void (*set_rotation) (XfceDisplaySettings *settings,
                          guint output_id,
                          RotationFlags rotation);
    RotationFlags (*get_rotations) (XfceDisplaySettings *settings,
                                    guint output_id);
    gdouble (*get_scale) (XfceDisplaySettings *settings,
                          guint output_id);
    void (*set_scale) (XfceDisplaySettings *settings,
                       guint output_id,
                       gdouble scale);
    void (*set_mode) (XfceDisplaySettings *settings,
                      guint output_id,
                      guint mode_id);
    void (*update_output_mode) (XfceDisplaySettings *settings,
                                XfceOutput *output,
                                guint mode_id);
    void (*set_position) (XfceDisplaySettings *settings,
                          guint output_id,
                          gint x,
                          gint y);
    XfceOutput *(*get_output) (XfceDisplaySettings *settings,
                               guint output_id);
    gboolean (*is_active) (XfceDisplaySettings *settings,
                           guint output_id);
    void (*set_active) (XfceDisplaySettings *settings,
                        guint output_id,
                        gboolean active);
    void (*update_output_active) (XfceDisplaySettings *settings,
                                  XfceOutput *output,
                                  gboolean active);
    gboolean (*is_primary) (XfceDisplaySettings *settings,
                            guint output_id);
    void (*set_primary) (XfceDisplaySettings *settings,
                         guint output_id,
                         gboolean primary);
    gboolean (*is_clonable) (XfceDisplaySettings *settings);
    void (*save) (XfceDisplaySettings *settings,
                  const gchar *scheme);
    void (*mirror) (XfceDisplaySettings *settings);
    void (*unmirror) (XfceDisplaySettings *settings);
    void (*update_output_mirror) (XfceDisplaySettings *settings,
                                  XfceOutput *output);
    void (*extend) (XfceDisplaySettings *settings,
                    guint output_id_1,
                    guint output_id_2,
                    ExtendedMode mode);
};

XfceDisplaySettings *
xfce_display_settings_new (gboolean opt_minimal,
                           GError **error);
gboolean
xfce_display_settings_is_minimal (XfceDisplaySettings *settings);
void
xfce_display_settings_set_minimal (XfceDisplaySettings *settings,
                                   gboolean minimal);
GtkBuilder *
xfce_display_settings_get_builder (XfceDisplaySettings *settings);
XfconfChannel *
xfce_display_settings_get_channel (XfceDisplaySettings *settings);
GtkWidget *
xfce_display_settings_get_scroll_area (XfceDisplaySettings *settings);
GHashTable *
xfce_display_settings_get_popups (XfceDisplaySettings *settings);
GList *
xfce_display_settings_get_outputs (XfceDisplaySettings *settings);
void
xfce_display_settings_set_outputs (XfceDisplaySettings *settings);
void
xfce_display_settings_populate_profile_list (XfceDisplaySettings *settings);
void
xfce_display_settings_populate_combobox (XfceDisplaySettings *settings);
void
xfce_display_settings_populate_popups (XfceDisplaySettings *settings);
void
xfce_display_settings_set_popups_visible (XfceDisplaySettings *settings,
                                          gboolean visible);
void
xfce_display_settings_reload (XfceDisplaySettings *settings);
guint
xfce_display_settings_get_selected_output_id (XfceDisplaySettings *settings);
void
xfce_display_settings_set_selected_output_id (XfceDisplaySettings *settings,
                                              guint output_id);

guint
xfce_display_settings_get_n_outputs (XfceDisplaySettings *settings);
guint
xfce_display_settings_get_n_active_outputs (XfceDisplaySettings *settings);
gchar **
xfce_display_settings_get_display_infos (XfceDisplaySettings *settings);
MirroredState
xfce_display_settings_get_mirrored_state (XfceDisplaySettings *settings);
GdkMonitor *
xfce_display_settings_get_monitor (XfceDisplaySettings *settings,
                                   guint output_id);
const gchar *
xfce_display_settings_get_friendly_name (XfceDisplaySettings *settings,
                                         guint output_id);
void
xfce_display_settings_get_geometry (XfceDisplaySettings *settings,
                                    guint output_id,
                                    GdkRectangle *geometry);
RotationFlags
xfce_display_settings_get_rotation (XfceDisplaySettings *settings,
                                    guint output_id);
void
xfce_display_settings_set_rotation (XfceDisplaySettings *settings,
                                    guint output_id,
                                    RotationFlags rotation);
RotationFlags
xfce_display_settings_get_rotations (XfceDisplaySettings *settings,
                                     guint output_id);
gdouble
xfce_display_settings_get_scale (XfceDisplaySettings *settings,
                                 guint output_id);
void
xfce_display_settings_set_scale (XfceDisplaySettings *settings,
                                 guint output_id,
                                 gdouble scale);
void
xfce_display_settings_set_mode (XfceDisplaySettings *settings,
                                guint output_id,
                                guint mode_id);
void
xfce_display_settings_set_position (XfceDisplaySettings *settings,
                                    guint output_id,
                                    gint x,
                                    gint y);
gboolean
xfce_display_settings_is_active (XfceDisplaySettings *settings,
                                 guint output_id);
void
xfce_display_settings_set_active (XfceDisplaySettings *settings,
                                  guint output_id,
                                  gboolean active);
gboolean
xfce_display_settings_is_primary (XfceDisplaySettings *settings,
                                  guint output_id);
void
xfce_display_settings_set_primary (XfceDisplaySettings *settings,
                                   guint output_id,
                                   gboolean primary);
gboolean
xfce_display_settings_is_mirrored (XfceDisplaySettings *settings,
                                   guint output_id_1,
                                   guint output_id_2);
ExtendedMode
xfce_display_settings_get_extended_mode (XfceDisplaySettings *settings,
                                         guint output_id_1,
                                         guint output_id_2);
gboolean
xfce_display_settings_is_clonable (XfceDisplaySettings *settings);
void
xfce_display_settings_save (XfceDisplaySettings *settings,
                            const gchar *scheme);
void
xfce_display_settings_mirror (XfceDisplaySettings *settings);
void
xfce_display_settings_unmirror (XfceDisplaySettings *settings);
void
xfce_display_settings_extend (XfceDisplaySettings *settings,
                              guint output_id_1,
                              guint output_id_2,
                              ExtendedMode mode);

G_END_DECLS

#endif /* !__DISPLAY_SETTINGS_H__ */
