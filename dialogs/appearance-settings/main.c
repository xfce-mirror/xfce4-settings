/*
 *  Copyright (c) 2008 Stephan Arts <stephan@xfce.org>
 *  Copyright (c) 2008 Jannis Pohlmann <jannis@xfce.org>
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

#ifdef HAVE_XFCE_REVISION_H
#include "xfce-revision.h"
#endif

#include "common/backlight.h"
#include "common/theme-variant.h"

#include <cairo-gobject.h>
#include <gio/gio.h>
#include <gtk/gtk.h>
#include <libxfce4ui/libxfce4ui.h>
#include <libxfce4util/libxfce4util.h>
#include <xfconf/xfconf.h>

#ifdef ENABLE_X11
#include <gdk/gdkx.h>
#include <gtk/gtkx.h>
#endif

#ifdef HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif

#define INCH_MM 25.4

/* Use a fallback DPI of 96 which should be ok-ish on most systems
 * and is only applied on rare occasions */
#define FALLBACK_DPI 96

/* Increase this number if new gtk settings have been added */
#define INITIALIZE_UINT (1)

enum
{
    COLUMN_THEME_PREVIEW,
    COLUMN_THEME_NAME,
    COLUMN_THEME_DISPLAY_NAME,
    COLUMN_THEME_COMMENT,
    COLUMN_THEME_WARNING,
    N_THEME_COLUMNS
};

enum
{
    COLUMN_RGBA_PIXBUF,
    COLUMN_RGBA_NAME,
    N_RGBA_COLUMNS
};

enum
{
    COLUMN_SCHEDULE_TIME,
    COLUMN_SCHEDULE_THEME,
    COLUMN_SCHEDULE_COLOR_SCHEME,
    COLUMN_SCHEDULE_XFWM4,
    COLUMN_SCHEDULE_ICON_THEME,
    COLUMN_SCHEDULE_CURSOR_THEME,
    COLUMN_SCHEDULE_BRIGHTNESS,
    COLUMN_SCHEDULE_COMMAND,
    N_SCHEDULE_COLUMNS
};

/* the choices of the Style and Color scheme columns */
enum
{
    COLUMN_CHOICE_LABEL,
    COLUMN_CHOICE_VALUE,
    N_CHOICE_COLUMNS
};

/* Theme schedule, applied by xfsettingsd */
#define SCHEDULE_ENABLED_PROP "/Xfce/ThemeSchedule/Enabled"

/* the rules are stored as one string array per column, see xfsettingsd/theme-schedule.c */
static const gchar *schedule_props[N_SCHEDULE_COLUMNS] = {
    "/Xfce/ThemeSchedule/Times",
    "/Xfce/ThemeSchedule/Themes",
    "/Xfce/ThemeSchedule/ColorSchemes",
    "/Xfce/ThemeSchedule/Xfwm4",
    "/Xfce/ThemeSchedule/IconThemes",
    "/Xfce/ThemeSchedule/CursorThemes",
    "/Xfce/ThemeSchedule/Brightness",
    "/Xfce/ThemeSchedule/Commands",
};

enum
{
    COLOR_FG,
    COLOR_BG,
    COLOR_SELECTED_BG,
    NUM_SYMBOLIC_COLORS
};

static const gchar *xft_hint_styles_array[] = {
    "hintnone", "hintslight", "hintmedium", "hintfull"
};

static const gchar *xft_rgba_array[] = {
    "none", "rgb", "bgr", "vrgb", "vbgr"
};

static const GtkTargetEntry theme_drop_targets[] = {
    { "text/uri-list", 0, 0 }
};

/* Option entries */
static gint opt_socket_id = 0;
static gboolean opt_version = FALSE;
static GOptionEntry option_entries[] = {
    { "socket-id", 's', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_INT, &opt_socket_id, N_ ("Settings manager socket"), N_ ("SOCKET ID") },
    { "version", 'v', 0, G_OPTION_ARG_NONE, &opt_version, N_ ("Version information"), NULL },
    { NULL }
};

/* Global xfconf channel */
static XfconfChannel *xsettings_channel;
static GSettings *desktop_interface_gsettings;
static GSettings *xapps_portal_gsettings;

static struct
{
    GtkListStore *store;
    GtkTreeModel *themes; /* the Gtk theme list of the Style tab */
    GtkTreeModel *icon_themes; /* the list of the Icons tab */
    GtkListStore *theme_choices;
    GtkListStore *color_scheme_choices;
    guint theme_choices_idle_id;
    GtkWidget *treeview;
    GtkTreeViewColumn *time_column;
    GtkTreeViewColumn *more_column;
    GtkWidget *status_label;
    GtkWidget *time_popover;
    GtkWidget *hour_spin;
    GtkWidget *minute_spin;
    gchar *time_path; /* the rule whose time the popover edits */
} schedule;

typedef struct
{
    GtkListStore *list_store;
    GtkTreeView *tree_view;
} preview_data;

typedef struct
{
    gatomicrefcount ref_count;

    GCancellable *cancellable;

    GtkListStore *list_store;
    GtkTreeView *tree_view;

    gchar *active_theme_name;
    gint scale_factor;

    GThreadPool *pool;
    GSList *check_list;
} icon_theme_preview_data;

typedef struct
{
    icon_theme_preview_data *itpd;
    gchar *icon_theme_name;
    GtkTreeRowReference *row;
    cairo_surface_t *preview;
} icon_theme_theme_preview_data;


static icon_theme_preview_data *icon_theme_preview_loading = NULL;


static void
install_theme (GtkWidget *widget,
               gchar **uris,
               GtkBuilder *builder);

static preview_data *
preview_data_new (GtkListStore *list_store,
                  GtkTreeView *tree_view)
{
    preview_data *pd;

    g_return_val_if_fail (list_store != NULL, NULL);
    g_return_val_if_fail (tree_view != NULL, NULL);
    g_return_val_if_fail (GTK_IS_LIST_STORE (list_store), NULL);
    g_return_val_if_fail (GTK_IS_TREE_VIEW (tree_view), NULL);

    pd = g_slice_new0 (preview_data);
    g_return_val_if_fail (pd != NULL, NULL);

    pd->list_store = list_store;
    pd->tree_view = tree_view;

    g_object_ref (G_OBJECT (pd->list_store));
    g_object_ref (G_OBJECT (pd->tree_view));

    return pd;
}

static void
preview_data_free (preview_data *pd)
{
    if (G_UNLIKELY (pd == NULL))
        return;
    g_object_unref (G_OBJECT (pd->list_store));
    g_object_unref (G_OBJECT (pd->tree_view));
    g_slice_free (preview_data, pd);
}

static icon_theme_preview_data *
icon_theme_preview_data_new (GtkListStore *list_store,
                             GtkTreeView *tree_view)
{
    icon_theme_preview_data *itpd;

    g_return_val_if_fail (GTK_IS_LIST_STORE (list_store), NULL);
    g_return_val_if_fail (GTK_IS_TREE_VIEW (tree_view), NULL);

    if (icon_theme_preview_loading != NULL)
    {
        g_cancellable_cancel (icon_theme_preview_loading->cancellable);
        icon_theme_preview_loading = NULL;
    }

    itpd = g_new0 (icon_theme_preview_data, 1);

    g_atomic_ref_count_init (&itpd->ref_count);
    itpd->cancellable = g_cancellable_new ();
    itpd->list_store = list_store;
    itpd->tree_view = tree_view;

    g_object_ref (G_OBJECT (itpd->list_store));
    g_object_ref (G_OBJECT (itpd->tree_view));

    icon_theme_preview_loading = itpd;

    return itpd;
}

static icon_theme_preview_data *
icon_theme_preview_data_ref (icon_theme_preview_data *itpd)
{
    g_return_val_if_fail (itpd != NULL, NULL);

    g_atomic_ref_count_inc (&itpd->ref_count);
    return itpd;
}

static void
icon_theme_preview_data_unref (icon_theme_preview_data *itpd)
{
    if (G_UNLIKELY (itpd == NULL))
        return;

    if (g_atomic_ref_count_dec (&itpd->ref_count))
    {
        if (icon_theme_preview_loading == itpd)
        {
            icon_theme_preview_loading = NULL;
        }

        g_object_unref (itpd->cancellable);
        g_object_unref (G_OBJECT (itpd->list_store));
        g_object_unref (G_OBJECT (itpd->tree_view));
        g_free (itpd->active_theme_name);
        g_thread_pool_free (itpd->pool, FALSE, FALSE);
        g_slist_free_full (itpd->check_list, g_free);
        g_free (itpd);
    }
}

static void
icon_theme_theme_preview_data_free (icon_theme_theme_preview_data *ittpd)
{
    g_free (ittpd->icon_theme_name);
    gtk_tree_row_reference_free (ittpd->row);
    if (ittpd->preview != NULL)
    {
        cairo_surface_destroy (ittpd->preview);
    }
    icon_theme_preview_data_unref (ittpd->itpd);
    g_free (ittpd);
}



static int
compute_xsettings_dpi (GtkWidget *widget)
{
    GdkScreen *screen;
    int width_mm, height_mm;
    int width, height;
    int dpi;

    G_GNUC_BEGIN_IGNORE_DEPRECATIONS
    screen = gtk_widget_get_screen (widget);
    width_mm = gdk_screen_get_width_mm (screen);
    height_mm = gdk_screen_get_height_mm (screen);
    dpi = FALLBACK_DPI;

    if (width_mm > 0 && height_mm > 0)
    {
        width = gdk_screen_get_width (screen);
        height = gdk_screen_get_height (screen);
        dpi = MIN (INCH_MM * width / width_mm,
                   INCH_MM * height / height_mm);
    }
    G_GNUC_END_IGNORE_DEPRECATIONS

    return dpi;
}

static void
theme_selection_changed (GtkTreeSelection *selection,
                         const gchar *property)
{
    GtkTreeModel *model;
    gboolean has_selection;
    gboolean has_xfwm4;
    gchar *name;
    GtkTreeIter iter;

    /* Get the selected list iter */
    has_selection = gtk_tree_selection_get_selected (selection, &model, &iter);
    if (G_LIKELY (has_selection))
    {
        has_xfwm4 = FALSE;

        /* Get the theme name and whether there is a xfwm4 theme as well */
        gtk_tree_model_get (model, &iter, COLUMN_THEME_NAME, &name, COLUMN_THEME_WARNING, &has_xfwm4, -1);

        /* Store the new theme */
        xfconf_channel_set_string (xsettings_channel, property, name);

        /* Set the matching xfwm4 theme if the selected theme is not an icon theme,
         * the xfconf setting is on, and a matching theme is available */
        if (xfconf_channel_get_bool (xsettings_channel, "/Xfce/SyncThemes", FALSE)
            && strcmp (property, "/Net/ThemeName") == 0)
        {
            if (!has_xfwm4)
                xfconf_channel_set_string (xfconf_channel_get ("xfwm4"), "/general/theme", name);

            /* Use the default theme if Adwaita is selected */
            else if (strcmp (name, "Adwaita") == 0 || strcmp (name, "Adwaita-dark") == 0)
                xfconf_channel_set_string (xfconf_channel_get ("xfwm4"), "/general/theme", "Default");
        }

        /* Cleanup */
        g_free (name);
    }
}

static void
cb_icon_theme_selection_changed (GtkTreeSelection *selection)
{
    /* Set the new icon theme */
    theme_selection_changed (selection, "/Net/IconThemeName");
}

static void
cb_ui_theme_selection_changed (GtkTreeSelection *selection)
{
    /* Set the new UI theme */
    theme_selection_changed (selection, "/Net/ThemeName");
}

static void
cb_xfwm4_sync_label_link_activated (GtkLabel *label)
{
    gchar *command;
    GError *error = NULL;

    command = g_find_program_in_path ("xfwm4-settings");
    if (command != NULL && !g_spawn_command_line_async (command, &error))
    {
        xfce_dialog_show_error (NULL, error, _("Failed to display Xfwm4 Settings"));
        g_error_free (error);
    }

    g_free (command);
}

static void
cb_xfwm4_sync_switch_toggled (GObject *self,
                              GParamSpec *pspec,
                              gpointer user_data)
{
    /* Set the new XFWM4 theme */
    if (gtk_switch_get_active (GTK_SWITCH (self)))
        theme_selection_changed (GTK_TREE_SELECTION (user_data), "/Net/ThemeName");
    else
        xfconf_channel_set_string (xfconf_channel_get ("xfwm4"), "/general/theme", "Default");
}

#ifdef ENABLE_X11
static void
cb_window_scaling_factor_combo_changed (GtkComboBox *combo)
{
    gint active;

    /* Get active item, prevent number outside the array (stay within zero-index) */
    active = CLAMP (gtk_combo_box_get_active (combo) + 1, 1, 2);

    /* Save setting */
    xfconf_channel_set_int (xsettings_channel, "/Gdk/WindowScalingFactor", active);
}

static void
cb_antialias_check_button_toggled (GtkToggleButton *toggle)
{
    gint active;

    /* Don't allow an inconsistent button anymore */
    gtk_toggle_button_set_inconsistent (toggle, FALSE);

    /* Get active */
    active = gtk_toggle_button_get_active (toggle) ? 1 : 0;

    /* Save setting */
    xfconf_channel_set_int (xsettings_channel, "/Xft/Antialias", active);
}

static void
cb_hinting_style_combo_changed (GtkComboBox *combo)
{
    gint active;

    /* Get active item, prevent number outside the array (stay within zero-index) */
    active = CLAMP (gtk_combo_box_get_active (combo), 0, (gint) G_N_ELEMENTS (xft_hint_styles_array) - 1);

    /* Save setting */
    xfconf_channel_set_string (xsettings_channel, "/Xft/HintStyle", xft_hint_styles_array[active]);

    /* Also update /Xft/Hinting to match */
    xfconf_channel_set_int (xsettings_channel, "/Xft/Hinting", active > 0 ? 1 : 0);
}

static void
cb_rgba_style_combo_changed (GtkComboBox *combo)
{
    gint active;

    /* Get active item, prevent number outside the array (stay within zero-index) */
    active = CLAMP (gtk_combo_box_get_active (combo), 0, (gint) G_N_ELEMENTS (xft_rgba_array) - 1);

    /* Save setting */
    xfconf_channel_set_string (xsettings_channel, "/Xft/RGBA", xft_rgba_array[active]);
}

static void
cb_custom_dpi_check_button_toggled (GtkToggleButton *custom_dpi_toggle,
                                    GtkSpinButton *custom_dpi_spin)
{
    gint dpi;

    if (gtk_toggle_button_get_active (custom_dpi_toggle))
    {
        /* Custom DPI is activated, so restore the last custom DPI we know about */
        dpi = xfconf_channel_get_int (xsettings_channel, "/Xfce/LastCustomDPI", -1);

        /* Unfortunately, we don't have a valid custom DPI value to use, so compute it */
        if (dpi <= 0)
            dpi = compute_xsettings_dpi (GTK_WIDGET (custom_dpi_toggle));

        /* Apply the computed custom DPI value */
        xfconf_channel_set_int (xsettings_channel, "/Xft/DPI", dpi);

        gtk_widget_set_sensitive (GTK_WIDGET (custom_dpi_spin), TRUE);
    }
    else
    {
        /* Custom DPI is deactivated, so remember the current value as the last custom DPI */
        dpi = gtk_spin_button_get_value_as_int (custom_dpi_spin);
        xfconf_channel_set_int (xsettings_channel, "/Xfce/LastCustomDPI", dpi);

        /* Tell xfsettingsd to compute the value itself */
        xfconf_channel_set_int (xsettings_channel, "/Xft/DPI", -1);

        /* Make the spin button insensitive */
        gtk_widget_set_sensitive (GTK_WIDGET (custom_dpi_spin), FALSE);
    }
}

static void
cb_custom_dpi_spin_button_changed (GtkSpinButton *custom_dpi_spin,
                                   GtkToggleButton *custom_dpi_toggle)
{
    gint dpi = gtk_spin_button_get_value_as_int (custom_dpi_spin);

    if (gtk_widget_is_sensitive (GTK_WIDGET (custom_dpi_spin)) && gtk_toggle_button_get_active (custom_dpi_toggle))
    {
        /* Custom DPI is turned on and the spin button has changed, so remember the value */
        xfconf_channel_set_int (xsettings_channel, "/Xfce/LastCustomDPI", dpi);
    }

    /* Tell xfsettingsd to apply the custom DPI value */
    xfconf_channel_set_int (xsettings_channel, "/Xft/DPI", dpi);
}
#endif

#ifdef ENABLE_SOUND_SETTINGS
static void
cb_sound_theme_combo_changed (GtkComboBox *combo,
                              gpointer user_data)
{
    /* Clear disk-based event sound cache (~/.cache/event-sound-cache*) */
    GFile *cache_dir = g_file_new_for_path (g_get_user_cache_dir ());
    GFileEnumerator *enumerator = g_file_enumerate_children (cache_dir,
                                                             G_FILE_ATTRIBUTE_STANDARD_NAME,
                                                             G_FILE_QUERY_INFO_NONE,
                                                             NULL, NULL);
    if (enumerator != NULL)
    {
        GFileInfo *info;
        while ((info = g_file_enumerator_next_file (enumerator, NULL, NULL)) != NULL)
        {
            const gchar *filename = g_file_info_get_name (info);

            /* libcanberra uses TDB files starting with this prefix */
            if (g_str_has_prefix (filename, "event-sound-cache"))
            {
                GFile *file_to_del = g_file_get_child (cache_dir, filename);
                g_file_delete (file_to_del, NULL, NULL);
                g_object_unref (file_to_del);
            }
            g_object_unref (info);
        }
        g_file_enumerator_close (enumerator, NULL, NULL);
        g_object_unref (enumerator);
    }
    g_object_unref (cache_dir);

    /* Flush PulseAudio/PipeWire RAM samples to force immediate theme reload
     * This pipe finds loaded samples and removes them from the sound server's memory */
    gchar *pactl = g_find_program_in_path ("pactl");
    if (pactl != NULL)
    {
        GError *error = NULL;
        gchar **argv = NULL;
        gchar *command = g_strdup_printf (
            "sh -c \"'%s' list short samples | cut -f2 | xargs -r -I {} '%s' remove-sample {}\"",
            pactl, pactl);

        if (!g_shell_parse_argv (command, NULL, &argv, &error))
        {
            g_warning ("Failed to parse pactl command: %s", error->message);
            g_clear_error (&error);
        }
        else
        {
            if (!g_spawn_async (NULL, argv, NULL,
                                G_SPAWN_SEARCH_PATH
                                    | G_SPAWN_STDOUT_TO_DEV_NULL
                                    | G_SPAWN_STDERR_TO_DEV_NULL,
                                NULL, NULL, NULL, &error))
            {
                g_warning ("Failed to start pactl cleanup process: %s", error->message);
                g_clear_error (&error);
            }
            g_strfreev (argv);
        }

        g_free (command);
        g_free (pactl);
    }
}

static void
add_sound_themes_from_dir (GtkComboBoxText *combo,
                           GHashTable *found_themes,
                           const gchar *base_dir)
{
    GDir *dir = g_dir_open (base_dir, 0, NULL);
    if (dir == NULL)
        return;

    const gchar *name;
    while ((name = g_dir_read_name (dir)) != NULL)
    {
        gchar *test_file = g_build_filename (base_dir, name, "index.theme", NULL);
        if (g_file_test (test_file, G_FILE_TEST_EXISTS))
        {
            if (!g_hash_table_contains (found_themes, name))
            {
                gtk_combo_box_text_append (combo, name, name);
                g_hash_table_add (found_themes, g_strdup (name));
            }
        }
        g_free (test_file);
    }
    g_dir_close (dir);
}

static void
appearance_settings_load_sound_themes (GtkComboBoxText *combo,
                                       GtkLabel *label_sound_theme)
{
    GHashTable *found_themes = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

    const gchar *user_data_dir = g_get_user_data_dir ();
    gchar *user_path = g_build_filename (user_data_dir, "sounds", NULL);
    add_sound_themes_from_dir (combo, found_themes, user_path);
    g_free (user_path);

    const gchar *const *system_dirs = g_get_system_data_dirs ();
    for (guint i = 0; system_dirs[i] != NULL; i++)
    {
        gchar *path = g_build_filename (system_dirs[i], "sounds", NULL);
        add_sound_themes_from_dir (combo, found_themes, path);
        g_free (path);
    }

    if (g_hash_table_size (found_themes) == 0)
    {
        const gchar *tooltip_text =
            _("No sound themes found. Install themes in /usr/share/sounds or ~/.local/share/sounds");
        gtk_widget_set_tooltip_text (GTK_WIDGET (combo), tooltip_text);
        gtk_widget_set_tooltip_text (GTK_WIDGET (label_sound_theme), tooltip_text);
    }
    else
    {
        gchar *theme = xfconf_channel_get_string (xsettings_channel, "/Net/SoundThemeName", "freedesktop");
        if (!gtk_combo_box_set_active_id (GTK_COMBO_BOX (combo), theme))
        {
            /* Invalid theme in settings -> select first item as a sane default */
            GtkTreeIter iter;
            GtkTreeModel *model = gtk_combo_box_get_model (GTK_COMBO_BOX (combo));
            if (gtk_tree_model_get_iter_first (model, &iter))
                gtk_combo_box_set_active_iter (GTK_COMBO_BOX (combo), &iter);
        }
        g_free (theme);
    }

    g_hash_table_destroy (found_themes);
}
#endif

static cairo_surface_t *
create_preview_image_surface (gint scale_factor)
{
    cairo_surface_t *preview = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, 44 * scale_factor, 44 * scale_factor);
    cairo_surface_set_device_scale (preview, scale_factor, scale_factor);
    return preview;
}

static gboolean
assign_icon_theme_preview (gpointer data)
{
    icon_theme_theme_preview_data *ittpd = data;

    if (!g_cancellable_is_cancelled (ittpd->itpd->cancellable))
    {
        GtkTreePath *tree_path = gtk_tree_row_reference_get_path (ittpd->row);
        if (tree_path != NULL)
        {
            GtkTreeIter iter;
            if (gtk_tree_model_get_iter (GTK_TREE_MODEL (ittpd->itpd->list_store), &iter, tree_path))
            {
                gtk_list_store_set (ittpd->itpd->list_store, &iter,
                                    COLUMN_THEME_PREVIEW, ittpd->preview,
                                    -1);
            }

            gtk_tree_path_free (tree_path);
        }
    }

    return FALSE;
}

static void
load_icon_theme_preview (gpointer data,
                         gpointer user_data)
{
    static const struct
    {
        const gchar *icon_name;
        int coords[2];
    } preview_icons[] = {
        { "folder", { 4, 4 } },
        { "go-down", { 24, 4 } },
        { "audio-volume-high", { 4, 24 } },
        { "web-browser", { 24, 24 } },
    };

    icon_theme_theme_preview_data *ittpd = data;
    icon_theme_preview_data *itpd = ittpd->itpd;

    if (!g_cancellable_is_cancelled (itpd->cancellable))
    {
        GtkIconTheme *icon_theme;
        cairo_t *cr;

        icon_theme = gtk_icon_theme_new ();
        gtk_icon_theme_set_custom_theme (icon_theme, ittpd->icon_theme_name);

        ittpd->preview = create_preview_image_surface (itpd->scale_factor);
        cr = cairo_create (ittpd->preview);

        for (gsize p = 0; p < G_N_ELEMENTS (preview_icons); p++)
        {
            GdkPixbuf *icon = NULL;
            if (gtk_icon_theme_has_icon (icon_theme, preview_icons[p].icon_name))
                icon = gtk_icon_theme_load_icon_for_scale (icon_theme, preview_icons[p].icon_name, 16, itpd->scale_factor, GTK_ICON_LOOKUP_FORCE_SIZE, NULL);
            else if (gtk_icon_theme_has_icon (icon_theme, "image-missing"))
                icon = gtk_icon_theme_load_icon_for_scale (icon_theme, "image-missing", 16, itpd->scale_factor, GTK_ICON_LOOKUP_FORCE_SIZE, NULL);

            if (icon)
            {
                cairo_save (cr);

                cairo_translate (cr, preview_icons[p].coords[0], preview_icons[p].coords[1]);
                cairo_scale (cr, 1.0 / itpd->scale_factor, 1.0 / itpd->scale_factor);

                gdk_cairo_set_source_pixbuf (cr, icon, 0, 0);
                cairo_paint (cr);

                cairo_restore (cr);
                g_object_unref (icon);
            }
        }

        cairo_destroy (cr);
        g_object_unref (icon_theme);
    }

    // NB: even if we're cancelled, we'd prefer the freeing/unreffing of the
    // preview data to happen on the main thread, so we'll still push an idle
    // function and free the data there.
    g_idle_add_full (G_PRIORITY_DEFAULT,
                     assign_icon_theme_preview,
                     ittpd,
                     (GDestroyNotify) icon_theme_theme_preview_data_free);
}

static void
handle_icon_theme (icon_theme_preview_data *itpd,
                   GFile *icon_theme_path)
{
    /* Build filename for the index.theme of the current icon theme directory */
    gchar *index_filename = g_build_filename (g_file_peek_path (icon_theme_path), "index.theme", NULL);

    /* Try to open the theme index file */
    XfceRc *index_file = xfce_rc_simple_open (index_filename, TRUE);

    gchar *file = g_file_get_basename (icon_theme_path);

    if (index_file != NULL
        && g_slist_find_custom (itpd->check_list, file, (GCompareFunc) g_utf8_collate) == NULL)
    {
        /* Set the icon theme group */
        xfce_rc_set_group (index_file, "Icon Theme");

        /* Check if the icon theme is valid and visible to the user */
        if (G_LIKELY (xfce_rc_has_entry (index_file, "Directories")
                      && !xfce_rc_read_bool_entry (index_file, "Hidden", FALSE)))
        {
            GError *error = NULL;
            gchar *warning_tooltip = NULL;
            cairo_surface_t *preview;
            const gchar *theme_name;
            const gchar *theme_comment;
            gchar *name_escaped;
            gchar *comment_escaped;
            gchar *visible_name;
            gchar *cache_filename;
            GtkTreeIter iter;
            GtkTreePath *tree_path;
            icon_theme_theme_preview_data *ittpd;

            /* Insert the theme in the check list */
            itpd->check_list = g_slist_prepend (itpd->check_list, g_strdup (file));

            /* Create the icon-theme preview (blank placeholder) */
            preview = create_preview_image_surface (itpd->scale_factor);

            /* Get translated icon theme name and comment */
            theme_name = xfce_rc_read_entry (index_file, "Name", file);
            theme_comment = xfce_rc_read_entry (index_file, "Comment", NULL);

            /* Escape the theme's name and comment, since they are markup, not text */
            name_escaped = g_markup_escape_text (theme_name, -1);
            comment_escaped = theme_comment ? g_markup_escape_text (theme_comment, -1) : NULL;
            visible_name = g_strdup_printf ("<b>%s</b>\n%s", name_escaped, comment_escaped);
            g_free (name_escaped);
            g_free (comment_escaped);

            /* Cache filename */
            cache_filename = g_build_filename (g_file_peek_path (icon_theme_path), "icon-theme.cache", NULL);

            if (!g_file_test (cache_filename, G_FILE_TEST_IS_REGULAR))
            {
                /* If the theme has no cache, mention this in the tooltip */
                warning_tooltip = g_strdup_printf (_("Warning: this icon theme has no cache file. You can create this by "
                                                     "running <i>gtk-update-icon-cache -f -t %s/</i> in a terminal emulator."),
                                                   g_file_peek_path (icon_theme_path));
            }
            else if (g_strcmp0 (file, "Adwaita") == 0 || g_strcmp0 (file, "HighContrast") == 0)
            {
                /* If the theme is known to be incomplete (does not follow fd.org standards), mention this in the tooltip */
                warning_tooltip = g_strdup (_("Warning: this icon theme is incomplete. Some icons will be missing."));
            }
            else if (g_strcmp0 (file, "gnome") == 0 || g_strcmp0 (file, "hicolor") == 0)
            {
                /* These actually are no full themes by purpose */
                warning_tooltip = g_strdup (_("Warning: this icon theme is incomplete. It only provides a base set of icons from which other themes can inherit."));
            }

            g_free (cache_filename);

            /* Append icon theme to the list store */
            gtk_list_store_append (itpd->list_store, &iter);
            gtk_list_store_set (itpd->list_store, &iter,
                                COLUMN_THEME_PREVIEW, preview,
                                COLUMN_THEME_NAME, file,
                                COLUMN_THEME_DISPLAY_NAME, visible_name,
                                COLUMN_THEME_WARNING, warning_tooltip != NULL,
                                COLUMN_THEME_COMMENT, warning_tooltip,
                                -1);
            tree_path = gtk_tree_model_get_path (GTK_TREE_MODEL (itpd->list_store), &iter);

            /* Check if this is the active theme, if so, select it */
            if (G_UNLIKELY (g_utf8_collate (file, itpd->active_theme_name) == 0))
            {
                gtk_tree_selection_select_path (gtk_tree_view_get_selection (itpd->tree_view), tree_path);
                gtk_tree_view_scroll_to_cell (itpd->tree_view, tree_path, NULL, TRUE, 0.5, 0);
            }

            cairo_surface_destroy (preview);

            ittpd = g_new0 (icon_theme_theme_preview_data, 1);
            ittpd->itpd = icon_theme_preview_data_ref (itpd);
            ittpd->icon_theme_name = g_strdup (file);
            ittpd->row = gtk_tree_row_reference_new (GTK_TREE_MODEL (itpd->list_store), tree_path);

            g_thread_pool_push (itpd->pool, ittpd, &error);
            if (error != NULL)
            {
                g_message ("Failed to push loading for icon theme '%s' onto thread pool: %s", file, error->message);
                g_error_free (error);
                icon_theme_theme_preview_data_free (ittpd);
            }

            gtk_tree_path_free (tree_path);
            g_free (visible_name);
            g_free (warning_tooltip);
        }
    }

    if (index_file != NULL)
    {
        xfce_rc_close (index_file);
    }
    g_free (file);
    g_free (index_filename);
}

static void
icon_theme_root_path_files_ready (GObject *source,
                                  GAsyncResult *res,
                                  gpointer user_data)
{
    icon_theme_preview_data *itpd = user_data;
    GFileEnumerator *enumerator = G_FILE_ENUMERATOR (source);
    GError *error = NULL;
    GList *file_infos = g_file_enumerator_next_files_finish (enumerator, res, &error);
    if (error != NULL)
    {
        GFile *parent = g_file_enumerator_get_container (enumerator);
        g_message ("Failed to read list of files from icon themes root: '%s': %s", g_file_peek_path (parent), error->message);
        g_error_free (error);
        g_object_unref (enumerator);
        icon_theme_preview_data_unref (itpd);
    }
    else if (file_infos == NULL)
    {
        g_object_unref (enumerator);
        icon_theme_preview_data_unref (itpd);
    }
    else
    {
        for (GList *l = file_infos; l != NULL; l = l->next)
        {
            GFileInfo *file_info = G_FILE_INFO (l->data);
            GFile *file = g_file_enumerator_get_child (enumerator, file_info);
            handle_icon_theme (itpd, file);
            g_object_unref (file);
        }

        g_list_free_full (file_infos, g_object_unref);

        // NB: don't re-ref itpd here; we'll "maintain" the ref that _enumerate_ready() took.
        g_file_enumerator_next_files_async (enumerator,
                                            20,
                                            G_PRIORITY_LOW,
                                            itpd->cancellable,
                                            icon_theme_root_path_files_ready,
                                            itpd);
    }
}

static void
icon_theme_root_path_enumerate_ready (GObject *source,
                                      GAsyncResult *res,
                                      gpointer user_data)
{
    icon_theme_preview_data *itpd = user_data;
    GFile *path = G_FILE (source);
    GError *error = NULL;
    GFileEnumerator *enumerator = g_file_enumerate_children_finish (path, res, &error);
    if (enumerator == NULL)
    {
        if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND))
        {
            g_message ("Failed to enumerate icon themes root '%s': %s", g_file_peek_path (path), error->message);
        }
        g_error_free (error);
    }
    else
    {
        if (icon_theme_preview_data_ref (itpd) != NULL)
        {
            g_file_enumerator_next_files_async (enumerator,
                                                20,
                                                G_PRIORITY_LOW,
                                                itpd->cancellable,
                                                icon_theme_root_path_files_ready,
                                                itpd);
        }
    }

    g_object_unref (path);
    icon_theme_preview_data_unref (itpd);
}


static gboolean
appearance_settings_load_icon_themes (gpointer user_data)
{
    icon_theme_preview_data *itpd = user_data;
    gchar **icon_theme_dirs;
    gsize i;
    GError *error = NULL;

    g_return_val_if_fail (itpd != NULL, FALSE);

    itpd->scale_factor = gtk_widget_get_scale_factor (GTK_WIDGET (itpd->tree_view));

    /* Determine current theme */
    itpd->active_theme_name = xfconf_channel_get_string (xsettings_channel, "/Net/IconThemeName", "Rodent");

    /* Determine directories to look in for icon themes */
    xfce_resource_push_path (XFCE_RESOURCE_ICONS, DATADIR G_DIR_SEPARATOR_S "icons");
    icon_theme_dirs = xfce_resource_dirs (XFCE_RESOURCE_ICONS);
    xfce_resource_pop_path (XFCE_RESOURCE_ICONS);

    itpd->pool = g_thread_pool_new (load_icon_theme_preview, NULL, 1, TRUE, &error);
    if (itpd->pool == NULL)
    {
        g_critical ("Failed to start thread pool for icon theme loading: %s", error->message);
        g_strfreev (icon_theme_dirs);
        g_error_free (error);
        return FALSE;
    }

    for (i = 0; icon_theme_dirs[i] != NULL; ++i)
    {
        GFile *path = g_file_new_for_path (icon_theme_dirs[i]);
        g_file_enumerate_children_async (path,
                                         "standard::",
                                         G_FILE_QUERY_INFO_NONE,
                                         G_PRIORITY_LOW,
                                         itpd->cancellable,
                                         icon_theme_root_path_enumerate_ready,
                                         icon_theme_preview_data_ref (itpd));
    }

    /* Free list of base directories */
    g_strfreev (icon_theme_dirs);

    return FALSE;
}

static gboolean
appearance_settings_load_ui_themes (gpointer user_data)
{
    preview_data *pd = user_data;
    GtkListStore *list_store;
    GtkTreeView *tree_view;
    GDir *dir;
    GtkTreePath *tree_path;
    GtkTreeIter iter;
    XfceRc *index_file;
    const gchar *file;
    gchar **ui_theme_dirs;
    gchar *index_filename;
    const gchar *theme_name;
    const gchar *theme_comment;
    gchar *active_theme_name;
    gchar *gtkrc_filename;
    gchar *gtkcss_filename;
    gchar *xfwm4_filename;
    gchar *notifyd_filename;
    gchar *theme_name_markup;
    gchar *comment_escaped;
    gint i;
    GSList *check_list = NULL;
    gboolean has_gtk2;
    gboolean has_xfwm4;
    gboolean has_notifyd;

    list_store = pd->list_store;
    tree_view = pd->tree_view;

    /* Determine current theme */
    active_theme_name = xfconf_channel_get_string (xsettings_channel, "/Net/ThemeName", "Default");

    /* Determine directories to look in for ui themes */
    xfce_resource_push_path (XFCE_RESOURCE_THEMES, DATADIR G_DIR_SEPARATOR_S "themes");
    ui_theme_dirs = xfce_resource_dirs (XFCE_RESOURCE_THEMES);
    xfce_resource_pop_path (XFCE_RESOURCE_THEMES);

    /* Iterate over all base directories */
    for (i = 0; ui_theme_dirs[i] != NULL; ++i)
    {
        /* Open directory handle */
        dir = g_dir_open (ui_theme_dirs[i], 0, NULL);

        /* Try next base directory if this one cannot be read */
        if (G_UNLIKELY (dir == NULL))
            continue;

        /* Iterate over filenames in the directory */
        while ((file = g_dir_read_name (dir)) != NULL)
        {
            /* Build the filenames for theme components */
            gtkrc_filename = g_build_filename (ui_theme_dirs[i], file, "gtk-2.0", "gtkrc", NULL);
            gtkcss_filename = g_build_filename (ui_theme_dirs[i], file, "gtk-3.0", "gtk.css", NULL);
            xfwm4_filename = g_build_filename (ui_theme_dirs[i], file, "xfwm4", "themerc", NULL);
            notifyd_filename = g_build_filename (ui_theme_dirs[i], file, "xfce-notify-4.0", "gtk.css", NULL);

            /* Check if the gtk.css file exists and the theme is not already in the list */
            if (g_file_test (gtkcss_filename, G_FILE_TEST_EXISTS)
                && g_slist_find_custom (check_list, file, (GCompareFunc) g_utf8_collate) == NULL)
            {
                /* Insert the theme in the check list */
                check_list = g_slist_prepend (check_list, g_strdup (file));

                /* Build filename for the index.theme of the current ui theme directory */
                index_filename = g_build_filename (ui_theme_dirs[i], file, "index.theme", NULL);

                /* Try to open the theme index file */
                index_file = xfce_rc_simple_open (index_filename, TRUE);

                if (G_LIKELY (index_file != NULL))
                {
                    /* Get translated ui theme name and comment */
                    theme_name = xfce_rc_read_entry (index_file, "Name", file);
                    theme_comment = xfce_rc_read_entry (index_file, "Comment", NULL);

                    /* Escape the comment because tooltips are markup, not text */
                    comment_escaped = theme_comment ? g_markup_escape_text (theme_comment, -1) : NULL;
                }
                else
                {
                    /* Set defaults */
                    theme_name = file;
                    comment_escaped = NULL;
                }

                /* Check if the gtk2 gtkrc, xfwm4 themerc, etc. files exist */
                has_gtk2 = FALSE;
                has_xfwm4 = FALSE;
                has_notifyd = FALSE;

                if (g_file_test (gtkrc_filename, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_REGULAR))
                    has_gtk2 = TRUE;
                if (g_file_test (xfwm4_filename, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_REGULAR))
                    has_xfwm4 = TRUE;
                if (g_file_test (notifyd_filename, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_REGULAR))
                    has_notifyd = TRUE;

                /* Compose the final markup text */
                theme_name_markup = g_strdup_printf ("<b>%s</b>\nGtk3", theme_name);

                if (has_gtk2)
                {
                    gchar *temp = g_strconcat (theme_name_markup, ", Gtk2", NULL);
                    g_free (theme_name_markup);
                    theme_name_markup = temp;
                }
                if (has_xfwm4)
                {
                    gchar *temp = g_strconcat (theme_name_markup, ", Xfwm4", NULL);
                    g_free (theme_name_markup);
                    theme_name_markup = temp;
                }
                if (has_notifyd)
                {
                    gchar *temp = g_strconcat (theme_name_markup, ", Xfce4-notifyd", NULL);
                    g_free (theme_name_markup);
                    theme_name_markup = temp;
                }

                /* Append ui theme to the list store */
                gtk_list_store_append (list_store, &iter);
                gtk_list_store_set (list_store, &iter,
                                    COLUMN_THEME_NAME, file,
                                    COLUMN_THEME_DISPLAY_NAME, theme_name_markup,
                                    COLUMN_THEME_WARNING, !has_xfwm4,
                                    COLUMN_THEME_COMMENT, comment_escaped, -1);

                /* Cleanup */
                if (G_LIKELY (index_file != NULL))
                    xfce_rc_close (index_file);
                g_free (comment_escaped);
                g_free (theme_name_markup);

                /* Check if this is the active theme, if so, select it */
                if (G_UNLIKELY (g_utf8_collate (file, active_theme_name) == 0))
                {
                    tree_path = gtk_tree_model_get_path (GTK_TREE_MODEL (list_store), &iter);
                    gtk_tree_selection_select_path (gtk_tree_view_get_selection (tree_view), tree_path);
                    gtk_tree_view_scroll_to_cell (tree_view, tree_path, NULL, TRUE, 0.5, 0);
                    gtk_tree_path_free (tree_path);
                }

                /* Free theme index filename */
                g_free (index_filename);
            }

            /* Free filenames */
            g_free (gtkrc_filename);
            g_free (gtkcss_filename);
            g_free (xfwm4_filename);
            g_free (notifyd_filename);
        }

        /* Close directory handle */
        g_dir_close (dir);
    }

    /* Free active theme name */
    g_free (active_theme_name);

    /* Free list of base directories */
    g_strfreev (ui_theme_dirs);

    /* Free the check list */
    g_slist_free_full (check_list, g_free);

    return FALSE;
}

static void
set_gsettings_color_scheme (GSettings *settings,
                            XfconfChannel *channel,
                            const gchar *property_name)
{
    if (settings != NULL)
    {
        GSettingsSchema *schema;
        g_object_get (settings, "settings-schema", &schema, NULL);
        if (g_settings_schema_has_key (schema, "color-scheme"))
        {
            gchar *str = xfconf_channel_get_string (channel, property_name, NULL);
            if (str != NULL && theme_variant_is_dark (str))
                g_settings_set_string (settings, "color-scheme", "prefer-dark");
            else
                g_settings_reset (settings, "color-scheme");
            g_free (str);
        }
        g_settings_schema_unref (schema);
    }
}

static void
appearance_settings_dialog_channel_property_changed (XfconfChannel *channel,
                                                     const gchar *property_name,
                                                     const GValue *value,
                                                     GtkBuilder *builder)
{
    GObject *object;
    gchar *str;
    guint i;
    gint antialias, dpi, custom_dpi;
    GtkTreeModel *model;

    g_return_if_fail (property_name != NULL);
    g_return_if_fail (GTK_IS_BUILDER (builder));

    if (strcmp (property_name, "/Xft/RGBA") == 0)
    {
        str = xfconf_channel_get_string (xsettings_channel, property_name, xft_rgba_array[0]);
        for (i = 0; i < G_N_ELEMENTS (xft_rgba_array); i++)
        {
            if (strcmp (str, xft_rgba_array[i]) == 0)
            {
                object = gtk_builder_get_object (builder, "xft_rgba_combo_box");
                gtk_combo_box_set_active (GTK_COMBO_BOX (object), i);
                break;
            }
        }
        g_free (str);
    }
    else if (strcmp (property_name, "/Gdk/WindowScalingFactor") == 0)
    {
        i = xfconf_channel_get_int (xsettings_channel, property_name, 1);
        object = gtk_builder_get_object (builder, "gdk_window_scaling_factor_combo_box");
        gtk_combo_box_set_active (GTK_COMBO_BOX (object), i - 1);
    }
    else if (strcmp (property_name, "/Xft/HintStyle") == 0)
    {
        str = xfconf_channel_get_string (xsettings_channel, property_name, xft_hint_styles_array[0]);
        for (i = 0; i < G_N_ELEMENTS (xft_hint_styles_array); i++)
        {
            if (strcmp (str, xft_hint_styles_array[i]) == 0)
            {
                object = gtk_builder_get_object (builder, "xft_hinting_style_combo_box");
                gtk_combo_box_set_active (GTK_COMBO_BOX (object), i);
                break;
            }
        }
        g_free (str);
    }
    else if (strcmp (property_name, "/Xft/Antialias") == 0)
    {
        object = gtk_builder_get_object (builder, "xft_antialias_check_button");
        antialias = xfconf_channel_get_int (xsettings_channel, property_name, -1);
        switch (antialias)
        {
            case 1:
                gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (object), TRUE);
                break;

            case 0:
                gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (object), FALSE);
                break;

            default: /* -1 */
                gtk_toggle_button_set_inconsistent (GTK_TOGGLE_BUTTON (object), TRUE);
                break;
        }
    }
    else if (strcmp (property_name, "/Xft/DPI") == 0)
    {
        /* The DPI has changed, so get its value and the last known custom value */
        dpi = xfconf_channel_get_int (xsettings_channel, property_name, FALLBACK_DPI);
        custom_dpi = xfconf_channel_get_int (xsettings_channel, "/Xfce/LastCustomDPI", -1);

        /* Activate the check button if we're using a custom DPI */
        object = gtk_builder_get_object (builder, "xft_custom_dpi_check_button");
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (object), dpi >= 0);

        /* If we're not using a custom DPI, compute the future custom DPI automatically */
        if (custom_dpi == -1)
            custom_dpi = compute_xsettings_dpi (GTK_WIDGET (object));

        object = gtk_builder_get_object (builder, "xft_custom_dpi_spin_button");

        if (dpi > 0)
        {
            /* We're using a custom DPI, so use the current DPI setting for the spin value */
            gtk_spin_button_set_value (GTK_SPIN_BUTTON (object), dpi);
        }
        else
        {
            /* Set the spin button value to the last custom DPI */
            gtk_spin_button_set_value (GTK_SPIN_BUTTON (object), custom_dpi);
        }
    }
    else if (strcmp (property_name, "/Net/ThemeName") == 0)
    {
        GtkTreeIter iter;

        object = gtk_builder_get_object (builder, "gtk_theme_treeview");
        model = gtk_tree_view_get_model (GTK_TREE_VIEW (object));

        if (gtk_tree_selection_get_selected (gtk_tree_view_get_selection (GTK_TREE_VIEW (object)),
                                             &model,
                                             &iter))
        {
            gchar *selected_name;
            gchar *new_name;

            gtk_tree_model_get (model, &iter, COLUMN_THEME_NAME, &selected_name, -1);

            new_name = xfconf_channel_get_string (channel, property_name, NULL);

            g_free (selected_name);
            g_free (new_name);
        }

        /* Set color scheme in gsettings, so that apps/services that read it apply the correct theme */
        set_gsettings_color_scheme (desktop_interface_gsettings, channel, property_name);
        set_gsettings_color_scheme (xapps_portal_gsettings, channel, property_name);
    }
    else if (strcmp (property_name, "/Net/IconThemeName") == 0)
    {
        GtkTreeIter iter;
        gboolean reload;

        reload = TRUE;

        object = gtk_builder_get_object (builder, "icon_theme_treeview");
        model = gtk_tree_view_get_model (GTK_TREE_VIEW (object));

        if (gtk_tree_selection_get_selected (gtk_tree_view_get_selection (GTK_TREE_VIEW (object)),
                                             &model,
                                             &iter))
        {
            gchar *selected_name;
            gchar *new_name;

            gtk_tree_model_get (model, &iter, COLUMN_THEME_NAME, &selected_name, -1);

            new_name = xfconf_channel_get_string (channel, property_name, NULL);

            reload = (strcmp (new_name, selected_name) != 0);

            g_free (selected_name);
            g_free (new_name);
        }


        if (reload)
        {
            icon_theme_preview_data *itpd;

            gtk_list_store_clear (GTK_LIST_STORE (model));
            itpd = icon_theme_preview_data_new (GTK_LIST_STORE (model), GTK_TREE_VIEW (object));
            if (itpd)
                g_idle_add_full (G_PRIORITY_DEFAULT_IDLE,
                                 appearance_settings_load_icon_themes,
                                 itpd,
                                 (GDestroyNotify) icon_theme_preview_data_unref);
        }
    }
    else if (strcmp (property_name, "/Gtk/MonospaceFontName") == 0)
    {
        /* Keep gsettings in sync */
        if (desktop_interface_gsettings != NULL)
        {
            str = xfconf_channel_get_string (channel, property_name, NULL);
            g_settings_set_string (desktop_interface_gsettings, "monospace-font-name", str);
            g_free (str);
        }
    }
}

static void
cb_theme_uri_dropped (GtkWidget *widget,
                      GdkDragContext *drag_context,
                      gint x,
                      gint y,
                      GtkSelectionData *data,
                      guint info,
                      guint timestamp,
                      GtkBuilder *builder)
{
    gchar **uris;

    uris = gtk_selection_data_get_uris (data);

    if (uris)
        install_theme (widget, uris, builder);
    else
        return;
}

static void
install_theme (GtkWidget *widget,
               gchar **uris,
               GtkBuilder *builder)
{
    gchar *argv[3];
    guint i;
    GError *error = NULL;
    gint status;
    GtkWidget *toplevel = gtk_widget_get_toplevel (widget);
    gchar *filename;
    GdkCursor *cursor;
    GdkWindow *gdkwindow;
    gboolean something_installed = FALSE;
    GObject *object;
    GtkTreeModel *model;
    preview_data *pd;
    icon_theme_preview_data *itpd;

    argv[0] = HELPERDIR G_DIR_SEPARATOR_S "appearance-install-theme";
    argv[2] = NULL;

    /* inform the user we are installing the theme */
    gdkwindow = gtk_widget_get_window (widget);
    cursor = gdk_cursor_new_from_name (gtk_widget_get_display (widget), "wait");
    if (cursor != NULL)
    {
        gdk_window_set_cursor (gdkwindow, cursor);
        g_object_unref (cursor);
    }

    /* iterate main loop to show cursor */
    while (gtk_events_pending ())
        gtk_main_iteration ();

    for (i = 0; uris[i] != NULL; i++)
    {
        filename = g_filename_from_uri (uris[i], NULL, NULL);
        if (filename == NULL)
            continue;

        argv[1] = filename;

        if (g_spawn_sync (NULL, argv, NULL, 0, NULL, NULL, NULL, NULL, &status, &error)
            && status > 0)
        {
            switch (WEXITSTATUS (status))
            {
                case 2:
                    g_set_error (&error, G_SPAWN_ERROR, 0,
                                 _("File is too large, installation aborted"));
                    break;

                case 3:
                    g_set_error_literal (&error, G_SPAWN_ERROR, 0,
                                         _("Failed to create temporary directory"));
                    break;

                case 4:
                    g_set_error_literal (&error, G_SPAWN_ERROR, 0,
                                         _("Failed to extract archive"));
                    break;

                case 5:
                    g_set_error_literal (&error, G_SPAWN_ERROR, 0,
                                         _("Unknown format, only archives and directories are supported"));
                    break;

                case 6:
                    g_set_error_literal (&error, G_SPAWN_ERROR, 0,
                                         _("Not a valid theme package"));
                    break;

                default:
                    g_set_error (&error, G_SPAWN_ERROR,
                                 0, _("An unknown error, exit code is %d"), WEXITSTATUS (status));
                    break;
            }
        }

        if (error != NULL)
        {
            xfce_dialog_show_error (GTK_WINDOW (toplevel), error, _("Failed to install theme"));
            g_clear_error (&error);
        }
        else
        {
            something_installed = TRUE;
        }

        g_free (filename);
    }

    g_strfreev (uris);
    gdk_window_set_cursor (gdkwindow, NULL);

    if (something_installed)
    {
        /* reload icon theme treeview in an idle loop */
        object = gtk_builder_get_object (builder, "icon_theme_treeview");
        model = gtk_tree_view_get_model (GTK_TREE_VIEW (object));
        gtk_list_store_clear (GTK_LIST_STORE (model));
        itpd = icon_theme_preview_data_new (GTK_LIST_STORE (model), GTK_TREE_VIEW (object));
        if (itpd)
            g_idle_add_full (G_PRIORITY_DEFAULT_IDLE,
                             appearance_settings_load_icon_themes,
                             itpd,
                             (GDestroyNotify) icon_theme_preview_data_unref);

        /* reload gtk theme treeview */
        object = gtk_builder_get_object (builder, "gtk_theme_treeview");
        model = gtk_tree_view_get_model (GTK_TREE_VIEW (object));
        gtk_list_store_clear (GTK_LIST_STORE (model));
        pd = preview_data_new (GTK_LIST_STORE (model), GTK_TREE_VIEW (object));
        if (pd)
            g_idle_add_full (G_PRIORITY_HIGH_IDLE,
                             appearance_settings_load_ui_themes,
                             pd,
                             (GDestroyNotify) preview_data_free);
    }
}

static void
appearance_settings_install_theme_cb (GtkButton *widget,
                                      GtkBuilder *builder)
{
    GtkWidget *window;
    GtkWidget *dialog;
    GtkFileChooserAction action = GTK_FILE_CHOOSER_ACTION_OPEN;
    GtkFileFilter *filter;
    gint res;
    gchar *theme;
    gchar *title;

    window = gtk_widget_get_toplevel (GTK_WIDGET (widget));
    g_object_get (G_OBJECT (widget), "name", &theme, NULL);
    title = g_strdup_printf (_("Install %s theme"), theme);
    dialog = gtk_file_chooser_dialog_new (title, GTK_WINDOW (window),
                                          action, _("_Cancel"),
                                          GTK_RESPONSE_CANCEL, _("_Open"),
                                          GTK_RESPONSE_ACCEPT, NULL);
    filter = gtk_file_filter_new ();
    gtk_file_filter_add_pattern (filter, "*.tar*");
    gtk_file_filter_add_pattern (filter, "*.zip");
    gtk_file_chooser_set_filter (GTK_FILE_CHOOSER (dialog), filter);
    gtk_file_chooser_set_select_multiple (GTK_FILE_CHOOSER (dialog), FALSE);

    res = gtk_dialog_run (GTK_DIALOG (dialog));
    if (res == GTK_RESPONSE_ACCEPT)
    {
        gchar *filename;
        gchar **uris;
        GtkFileChooser *chooser = GTK_FILE_CHOOSER (dialog);

        uris = g_new0 (gchar *, 2);
        filename = gtk_file_chooser_get_filename (chooser);
        uris[0] = g_filename_to_uri (filename, NULL, NULL);
        install_theme (window, uris, builder);
        g_free (filename);
    }

    gtk_widget_destroy (dialog);
    g_free (title);
    g_free (theme);
}

#ifdef ENABLE_X11
static cairo_surface_t *
appearance_settings_draw_subpixel_icon (gboolean is_rgb,
                                        gboolean is_vertical,
                                        gint size,
                                        gint scale_factor)
{
    cairo_surface_t *surface;
    cairo_t *cr;
    gint color_width;
    double colors[3][3] = { { 0.0 } };

    surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, size * scale_factor, size * scale_factor);
    cairo_surface_set_device_scale (surface, scale_factor, scale_factor);

    cr = cairo_create (surface);
    cairo_scale (cr, 1.0 / scale_factor, 1.0 / scale_factor);

    if (is_rgb)
    {
        colors[0][0] = 1.0; // red
        colors[1][1] = 1.0; // green
        colors[2][2] = 1.0; // blue
    }
    else
    {
        colors[0][2] = 1.0; // blue
        colors[1][1] = 1.0; // green
        colors[2][0] = 1.0; // red
    }

    color_width = (size * scale_factor) / 3;

    for (gsize i = 0; i < G_N_ELEMENTS (colors); ++i)
    {
        cairo_save (cr);
        cairo_set_source_rgb (cr, colors[i][0], colors[i][1], colors[i][2]);
        cairo_rectangle (cr,
                         is_vertical ? 0 : color_width * i,
                         is_vertical ? color_width * i : 0,
                         is_vertical ? size * scale_factor : color_width,
                         is_vertical ? color_width : size * scale_factor);
        cairo_fill (cr);
        cairo_restore (cr);
    }

    cairo_destroy (cr);

    return surface;
}
#endif

static void
appearance_settings_dialog_configure_widgets (GtkBuilder *builder)
{
    GObject *object;
    GtkListStore *list_store;
    GtkCellRenderer *renderer;
    GtkTreeSelection *selection;
    GtkTreeViewColumn *column;
    icon_theme_preview_data *itpd;
    preview_data *pd;
    gchar *path;

    /* Icon themes list */
    object = gtk_builder_get_object (builder, "install_icon_theme");
    g_object_set (object, "name", "icon", NULL);
    g_signal_connect (G_OBJECT (object), "clicked", G_CALLBACK (appearance_settings_install_theme_cb), builder);

    object = gtk_builder_get_object (builder, "icon_theme_treeview");
    list_store = gtk_list_store_new (N_THEME_COLUMNS, CAIRO_GOBJECT_TYPE_SURFACE, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_BOOLEAN);
    gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (list_store), COLUMN_THEME_DISPLAY_NAME, GTK_SORT_ASCENDING);
    gtk_tree_view_set_model (GTK_TREE_VIEW (object), GTK_TREE_MODEL (list_store));
    gtk_tree_view_set_tooltip_column (GTK_TREE_VIEW (object), COLUMN_THEME_COMMENT);

    /* Single-column layout */
    column = gtk_tree_view_column_new ();

    /* Icon Previews */
    renderer = gtk_cell_renderer_pixbuf_new ();
    gtk_tree_view_column_pack_start (column, renderer, FALSE);
    gtk_tree_view_column_set_attributes (column, renderer, "surface", COLUMN_THEME_PREVIEW, NULL);
    gtk_tree_view_append_column (GTK_TREE_VIEW (object), column);

    /* Theme Name and Description */
    renderer = gtk_cell_renderer_text_new ();
    gtk_tree_view_column_pack_start (column, renderer, TRUE);
    gtk_tree_view_column_set_attributes (column, renderer, "markup", COLUMN_THEME_DISPLAY_NAME, NULL);
    g_object_set (G_OBJECT (renderer), "ellipsize", PANGO_ELLIPSIZE_END, NULL);

    /* Warning Icon */
    renderer = gtk_cell_renderer_pixbuf_new ();
    gtk_tree_view_column_pack_start (column, renderer, FALSE);
    gtk_tree_view_column_set_attributes (column, renderer, "visible", COLUMN_THEME_WARNING, NULL);
    g_object_set (G_OBJECT (renderer), "icon-name", "dialog-warning", NULL);

    itpd = icon_theme_preview_data_new (GTK_LIST_STORE (list_store), GTK_TREE_VIEW (object));
    if (itpd)
        g_idle_add_full (G_PRIORITY_DEFAULT_IDLE,
                         appearance_settings_load_icon_themes,
                         itpd,
                         (GDestroyNotify) icon_theme_preview_data_unref);

    g_object_unref (G_OBJECT (list_store));

    selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (object));
    gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);
    gtk_tree_view_set_activate_on_single_click (GTK_TREE_VIEW (object), TRUE);
    g_signal_connect_swapped (G_OBJECT (object), "row-activated", G_CALLBACK (cb_icon_theme_selection_changed), selection);

    gtk_drag_dest_set (GTK_WIDGET (object), GTK_DEST_DEFAULT_ALL,
                       theme_drop_targets, G_N_ELEMENTS (theme_drop_targets),
                       GDK_ACTION_COPY);
    g_signal_connect (G_OBJECT (object), "drag-data-received", G_CALLBACK (cb_theme_uri_dropped), builder);

    /* Gtk (UI) themes */
    object = gtk_builder_get_object (builder, "gtk_theme_treeview");

    list_store = gtk_list_store_new (N_THEME_COLUMNS, GDK_TYPE_PIXBUF, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_BOOLEAN);
    gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (list_store), COLUMN_THEME_DISPLAY_NAME, GTK_SORT_ASCENDING);
    gtk_tree_view_set_model (GTK_TREE_VIEW (object), GTK_TREE_MODEL (list_store));
    gtk_tree_view_set_tooltip_column (GTK_TREE_VIEW (object), COLUMN_THEME_COMMENT);

    /* Single-column layout */
    column = gtk_tree_view_column_new ();

    /* Icon Previews */
    renderer = gtk_cell_renderer_pixbuf_new ();
    gtk_tree_view_column_pack_start (column, renderer, FALSE);
    gtk_tree_view_column_set_attributes (column, renderer, "pixbuf", COLUMN_THEME_PREVIEW, NULL);
    gtk_tree_view_append_column (GTK_TREE_VIEW (object), column);

    /* Theme Name and Description */
    renderer = gtk_cell_renderer_text_new ();
    gtk_tree_view_column_pack_start (column, renderer, TRUE);
    gtk_tree_view_column_set_attributes (column, renderer, "markup", COLUMN_THEME_DISPLAY_NAME, NULL);
    g_object_set (G_OBJECT (renderer), "ellipsize", PANGO_ELLIPSIZE_END, NULL);

    pd = preview_data_new (list_store, GTK_TREE_VIEW (object));
    if (pd)
        g_idle_add_full (G_PRIORITY_HIGH_IDLE,
                         appearance_settings_load_ui_themes,
                         pd,
                         (GDestroyNotify) preview_data_free);

    g_object_unref (G_OBJECT (list_store));

    selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (object));
    gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);
    gtk_tree_view_set_activate_on_single_click (GTK_TREE_VIEW (object), TRUE);
    g_signal_connect_swapped (G_OBJECT (object), "row-activated", G_CALLBACK (cb_ui_theme_selection_changed), selection);

    gtk_drag_dest_set (GTK_WIDGET (object), GTK_DEST_DEFAULT_ALL,
                       theme_drop_targets, G_N_ELEMENTS (theme_drop_targets),
                       GDK_ACTION_COPY);
    g_signal_connect (G_OBJECT (object), "drag-data-received", G_CALLBACK (cb_theme_uri_dropped), builder);
    object = gtk_builder_get_object (builder, "install_gtk_theme");
    g_object_set (object, "name", "Gtk", NULL);
    g_signal_connect (G_OBJECT (object), "clicked", G_CALLBACK (appearance_settings_install_theme_cb), builder);

    /* Switch for xfwm4 theme matching, gets hidden if xfwm4 is not installed */
    path = g_find_program_in_path ("xfwm4");
    if (path != NULL)
    {
        object = gtk_builder_get_object (builder, "xfwm4_sync_switch");
        xfconf_g_property_bind (xsettings_channel, "/Xfce/SyncThemes", G_TYPE_BOOLEAN, G_OBJECT (object), "active");
        g_signal_connect (G_OBJECT (object), "notify::active", G_CALLBACK (cb_xfwm4_sync_switch_toggled), selection);

        object = gtk_builder_get_object (builder, "xfwm4_sync_label");
        g_signal_connect (G_OBJECT (object), "activate-link", G_CALLBACK (cb_xfwm4_sync_label_link_activated), NULL);
    }
    else
    {
        object = gtk_builder_get_object (builder, "xfwm4_sync");
        gtk_widget_hide (GTK_WIDGET (object));
    }
    g_free (path);

    /* Enable buttons in native GTK dialog headers */
    object = gtk_builder_get_object (builder, "gtk_dialog_button_header_check_button");
    xfconf_g_property_bind (xsettings_channel, "/Gtk/DialogsUseHeader", G_TYPE_BOOLEAN,
                            G_OBJECT (object), "active");

    /* Enable editable menu accelerators */
    object = gtk_builder_get_object (builder, "gtk_caneditaccels_check_button");
    xfconf_g_property_bind (xsettings_channel, "/Gtk/CanChangeAccels", G_TYPE_BOOLEAN,
                            G_OBJECT (object), "active");

    /* Show menu images */
    object = gtk_builder_get_object (builder, "gtk_menu_images_check_button");
    xfconf_g_property_bind (xsettings_channel, "/Gtk/MenuImages", G_TYPE_BOOLEAN,
                            G_OBJECT (object), "active");

    /* Show button images */
    object = gtk_builder_get_object (builder, "gtk_button_images_check_button");
    xfconf_g_property_bind (xsettings_channel, "/Gtk/ButtonImages", G_TYPE_BOOLEAN,
                            G_OBJECT (object), "active");

    /* Font name */
    object = gtk_builder_get_object (builder, "gtk_fontname_button");
    xfconf_g_property_bind (xsettings_channel, "/Gtk/FontName", G_TYPE_STRING,
                            G_OBJECT (object), "font-name");

    /* Monospace font name */
    object = gtk_builder_get_object (builder, "gtk_monospace_fontname_button");
    xfconf_g_property_bind (xsettings_channel, "/Gtk/MonospaceFontName", G_TYPE_STRING,
                            G_OBJECT (object), "font-name");

#ifdef ENABLE_X11
    if (GDK_IS_X11_DISPLAY (gdk_display_get_default ()))
    {
        GObject *object2;
        cairo_surface_t *surface;
        gint menu_icon_size;
        gint scale_factor = gtk_widget_get_scale_factor (GTK_WIDGET (gtk_builder_get_object (builder, "icon_theme_treeview")));

        /* Hinting style */
        object = gtk_builder_get_object (builder, "xft_hinting_style_combo_box");
        appearance_settings_dialog_channel_property_changed (xsettings_channel, "/Xft/HintStyle", NULL, builder);
        g_signal_connect (G_OBJECT (object), "changed", G_CALLBACK (cb_hinting_style_combo_changed), NULL);

        /* Hinting */
        object = gtk_builder_get_object (builder, "xft_antialias_check_button");
        appearance_settings_dialog_channel_property_changed (xsettings_channel, "/Xft/Antialias", NULL, builder);
        g_signal_connect (G_OBJECT (object), "toggled", G_CALLBACK (cb_antialias_check_button_toggled), NULL);

        /* Subpixel (rgba) hinting Combo */
        object = gtk_builder_get_object (builder, "xft_rgba_store");

        gtk_icon_size_lookup (GTK_ICON_SIZE_MENU, &menu_icon_size, &menu_icon_size);

        surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, menu_icon_size * scale_factor, menu_icon_size * scale_factor);
        cairo_surface_set_device_scale (surface, scale_factor, scale_factor);
        gtk_list_store_insert_with_values (GTK_LIST_STORE (object), NULL, 0, 0, surface, 1, _("None"), -1);
        cairo_surface_destroy (surface);

        surface = appearance_settings_draw_subpixel_icon (TRUE, FALSE, menu_icon_size, scale_factor);
        gtk_list_store_insert_with_values (GTK_LIST_STORE (object), NULL, 1, 0, surface, 1, _("RGB"), -1);
        cairo_surface_destroy (surface);

        surface = appearance_settings_draw_subpixel_icon (FALSE, FALSE, menu_icon_size, scale_factor);
        gtk_list_store_insert_with_values (GTK_LIST_STORE (object), NULL, 2, 0, surface, 1, _("BGR"), -1);
        cairo_surface_destroy (surface);

        surface = appearance_settings_draw_subpixel_icon (TRUE, TRUE, menu_icon_size, scale_factor);
        gtk_list_store_insert_with_values (GTK_LIST_STORE (object), NULL, 3, 0, surface, 1, _("Vertical RGB"), -1);
        cairo_surface_destroy (surface);

        surface = appearance_settings_draw_subpixel_icon (FALSE, TRUE, menu_icon_size, scale_factor);
        gtk_list_store_insert_with_values (GTK_LIST_STORE (object), NULL, 4, 0, surface, 1, _("Vertical BGR"), -1);
        cairo_surface_destroy (surface);

        object = gtk_builder_get_object (builder, "xft_rgba_combo_box");
        appearance_settings_dialog_channel_property_changed (xsettings_channel, "/Xft/RGBA", NULL, builder);
        g_signal_connect (G_OBJECT (object), "changed", G_CALLBACK (cb_rgba_style_combo_changed), NULL);

        /* DPI */
        object = gtk_builder_get_object (builder, "xft_custom_dpi_check_button");
        object2 = gtk_builder_get_object (builder, "xft_custom_dpi_spin_button");
        appearance_settings_dialog_channel_property_changed (xsettings_channel, "/Xft/DPI", NULL, builder);
        gtk_widget_set_sensitive (GTK_WIDGET (object2), gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (object)));
        g_signal_connect (G_OBJECT (object), "toggled", G_CALLBACK (cb_custom_dpi_check_button_toggled), object2);
        g_signal_connect (G_OBJECT (object2), "value-changed", G_CALLBACK (cb_custom_dpi_spin_button_changed), object);

        /* Window scaling factor */
        object = gtk_builder_get_object (builder, "gdk_window_scaling_factor_combo_box");
        appearance_settings_dialog_channel_property_changed (xsettings_channel, "/Gdk/WindowScalingFactor", NULL, builder);
        g_signal_connect (G_OBJECT (object), "changed", G_CALLBACK (cb_window_scaling_factor_combo_changed), NULL);
    }
    else
#endif
    {
        gtk_widget_hide (GTK_WIDGET (gtk_builder_get_object (builder, "frame4")));
        gtk_widget_hide (GTK_WIDGET (gtk_builder_get_object (builder, "frame5")));
        gtk_widget_hide (GTK_WIDGET (gtk_builder_get_object (builder, "frame6")));
    }

#ifdef ENABLE_SOUND_SETTINGS
    /* Ensure canberra-gtk-module is in /Gtk/Modules (Xfconf Robust Check) */
    gchar *modules = xfconf_channel_get_string (xsettings_channel, "/Gtk/Modules", "");
    gchar **module_array = g_strsplit (modules, ":", -1);

    /* Check if the module is already in the list */
    if (!g_strv_contains ((const gchar *const *) module_array, "canberra-gtk-module"))
    {
        gchar *new_modules = xfce_str_is_empty (modules) ? g_strdup ("canberra-gtk-module")
                                                         : g_strdup_printf ("%s:canberra-gtk-module", modules);
        xfconf_channel_set_string (xsettings_channel, "/Gtk/Modules", new_modules);
        g_free (new_modules);
    }

    g_strfreev (module_array);
    g_free (modules);

    /* Sounds */
    object = gtk_builder_get_object (builder, "event_sounds_frame");
    gtk_widget_show (GTK_WIDGET (object));

    GObject *enable_sounds = gtk_builder_get_object (builder, "enable_event_sounds_check_button");
    GObject *feedback_sounds = gtk_builder_get_object (builder, "enable_input_feedback_sounds_button");
    GObject *sound_combo = gtk_builder_get_object (builder, "xfce_sound_theme_combo_box");
    GObject *label_sound_theme = gtk_builder_get_object (builder, "label_sound_theme");

    xfconf_g_property_bind (xsettings_channel, "/Net/SoundThemeName", G_TYPE_STRING, sound_combo, "active-id");
    appearance_settings_load_sound_themes (GTK_COMBO_BOX_TEXT (sound_combo), GTK_LABEL (label_sound_theme));
    g_signal_connect (sound_combo, "changed", G_CALLBACK (cb_sound_theme_combo_changed), NULL);

    xfconf_g_property_bind (xsettings_channel, "/Net/EnableEventSounds", G_TYPE_BOOLEAN,
                            enable_sounds, "active");
    xfconf_g_property_bind (xsettings_channel, "/Net/EnableInputFeedbackSounds", G_TYPE_BOOLEAN,
                            feedback_sounds, "active");
    g_object_bind_property (enable_sounds, "active",
                            feedback_sounds, "sensitive",
                            G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);
    g_object_bind_property (enable_sounds, "active",
                            sound_combo, "sensitive",
                            G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);
#endif
}

/* "HH:MM" -> minutes since midnight, or -1 if invalid */
static gint
schedule_parse_time (const gchar *str)
{
    guint64 hours, minutes;
    gchar *end;

    if (str == NULL || !g_ascii_isdigit (*str))
        return -1;

    hours = g_ascii_strtoull (str, &end, 10);
    if (*end != ':' || !g_ascii_isdigit (end[1]))
        return -1;

    minutes = g_ascii_strtoull (end + 1, &end, 10);
    if (*end != '\0' || hours > 23 || minutes > 59)
        return -1;

    return hours * 60 + minutes;
}

/* the installed theme whose name matches @name case-insensitively, or NULL */
static gchar *
schedule_find_theme (const gchar *name,
                     gpointer user_data)
{
    GtkTreeModel *model = schedule.themes;
    GtkTreeIter iter;
    gchar *found = NULL;

    if (gtk_tree_model_get_iter_first (model, &iter))
    {
        do
        {
            gchar *theme;

            gtk_tree_model_get (model, &iter, COLUMN_THEME_NAME, &theme, -1);
            if (theme != NULL && g_ascii_strcasecmp (theme, name) == 0)
                found = g_steal_pointer (&theme);
            g_free (theme);
        } while (found == NULL && gtk_tree_model_iter_next (model, &iter));
    }

    return found;
}

/* the value of the choice labelled @label in @choices, or NULL */
static gchar *
schedule_choice_value (GtkListStore *choices,
                       const gchar *label)
{
    GtkTreeModel *model = GTK_TREE_MODEL (choices);
    GtkTreeIter iter;
    gchar *value = NULL;

    if (gtk_tree_model_get_iter_first (model, &iter))
    {
        do
        {
            gchar *choice_label;

            gtk_tree_model_get (model, &iter, COLUMN_CHOICE_LABEL, &choice_label, -1);
            if (g_strcmp0 (choice_label, label) == 0)
                gtk_tree_model_get (model, &iter, COLUMN_CHOICE_VALUE, &value, -1);
            g_free (choice_label);
        } while (value == NULL && gtk_tree_model_iter_next (model, &iter));
    }

    return value;
}

/* the label of the choice with @value in @choices, or @value itself */
static gchar *
schedule_choice_label (GtkListStore *choices,
                       const gchar *value)
{
    GtkTreeModel *model = GTK_TREE_MODEL (choices);
    GtkTreeIter iter;
    gchar *label = NULL;

    if (gtk_tree_model_get_iter_first (model, &iter))
    {
        do
        {
            gchar *choice_value;

            gtk_tree_model_get (model, &iter, COLUMN_CHOICE_VALUE, &choice_value, -1);
            if (g_strcmp0 (choice_value, value) == 0)
                gtk_tree_model_get (model, &iter, COLUMN_CHOICE_LABEL, &label, -1);
            g_free (choice_value);
        } while (label == NULL && gtk_tree_model_iter_next (model, &iter));
    }

    return label != NULL ? label : g_strdup (value);
}

/* what the rule at @iter does, for the status line, or NULL if nothing */
static gchar *
schedule_describe_rule (GtkTreeModel *model,
                        GtkTreeIter *iter)
{
    GPtrArray *parts = g_ptr_array_new_with_free_func (g_free);
    gchar *theme, *color_scheme, *icon_theme, *cursor_theme, *brightness, *command;
    gint n_more = 0;
    gchar *text = NULL;

    gtk_tree_model_get (model, iter,
                        COLUMN_SCHEDULE_THEME, &theme,
                        COLUMN_SCHEDULE_COLOR_SCHEME, &color_scheme,
                        COLUMN_SCHEDULE_ICON_THEME, &icon_theme,
                        COLUMN_SCHEDULE_CURSOR_THEME, &cursor_theme,
                        COLUMN_SCHEDULE_BRIGHTNESS, &brightness,
                        COLUMN_SCHEDULE_COMMAND, &command,
                        -1);

    if (g_strcmp0 (theme, ":dark") == 0 || g_strcmp0 (theme, ":light") == 0)
    {
        gchar *current = xfconf_channel_get_string (xsettings_channel, "/Net/ThemeName", "Default");
        gchar *variant = theme_variant_find (current, theme[1] == 'd', schedule_find_theme, NULL);

        if (variant == NULL)
            g_ptr_array_add (parts, g_strdup_printf (theme[1] == 'd' ? _("no dark variant of %s")
                                                                     : _("no light variant of %s"),
                                                     current));
        else if (g_strcmp0 (variant, current) != 0)
            g_ptr_array_add (parts, g_strdup (variant));

        g_free (variant);
        g_free (current);
    }
    else if (!xfce_str_is_empty (theme))
        g_ptr_array_add (parts, g_strdup (theme));

    if (g_strcmp0 (color_scheme, "prefer-dark") == 0)
        g_ptr_array_add (parts, g_strdup (_("dark color scheme")));
    else if (g_strcmp0 (color_scheme, "prefer-light") == 0)
        g_ptr_array_add (parts, g_strdup (_("light color scheme")));
    else if (g_strcmp0 (color_scheme, "default") == 0)
        g_ptr_array_add (parts, g_strdup (_("default color scheme")));

    n_more = !xfce_str_is_empty (icon_theme)
             + !xfce_str_is_empty (cursor_theme)
             + !xfce_str_is_empty (brightness)
             + !xfce_str_is_empty (command);
    if (n_more > 0)
        g_ptr_array_add (parts, g_strdup_printf (g_dngettext (GETTEXT_PACKAGE, "%d more action", "%d more actions", n_more), n_more));

    if (parts->len > 0)
    {
        g_ptr_array_add (parts, NULL);
        text = g_strjoinv (", ", (gchar **) parts->pdata);
    }
    else if (!xfce_str_is_empty (theme) || !xfce_str_is_empty (color_scheme))
        text = g_strdup (_("no change"));

    g_ptr_array_unref (parts);
    g_free (theme);
    g_free (color_scheme);
    g_free (icon_theme);
    g_free (cursor_theme);
    g_free (brightness);
    g_free (command);

    return text;
}

static void
schedule_update_status (void)
{
    GtkTreeModel *model = GTK_TREE_MODEL (schedule.store);
    GtkTreeIter iter;
    GDateTime *now;
    gint now_minutes;
    gint next_minutes = -1, first_minutes = -1;
    gchar *next_rule = NULL, *first_rule = NULL;
    gchar *text;

    /* not built yet */
    if (schedule.status_label == NULL)
        return;

    if (!xfconf_channel_get_bool (xsettings_channel, SCHEDULE_ENABLED_PROP, FALSE))
    {
        gtk_label_set_text (GTK_LABEL (schedule.status_label), "");
        return;
    }

    now = g_date_time_new_now_local ();
    now_minutes = g_date_time_get_hour (now) * 60 + g_date_time_get_minute (now);
    g_date_time_unref (now);

    /* next rule after now, or the first rule of tomorrow */
    if (gtk_tree_model_get_iter_first (model, &iter))
    {
        do
        {
            gchar *time_str, *rule;
            gint minutes;

            gtk_tree_model_get (model, &iter, COLUMN_SCHEDULE_TIME, &time_str, -1);
            minutes = schedule_parse_time (time_str);
            rule = schedule_describe_rule (model, &iter);

            if (minutes >= 0 && rule != NULL)
            {
                if (minutes > now_minutes && (next_minutes < 0 || minutes < next_minutes))
                {
                    next_minutes = minutes;
                    g_set_str (&next_rule, rule);
                }
                if (first_minutes < 0 || minutes < first_minutes)
                {
                    first_minutes = minutes;
                    g_set_str (&first_rule, rule);
                }
            }

            g_free (time_str);
            g_free (rule);
        } while (gtk_tree_model_iter_next (model, &iter));
    }

    if (next_minutes < 0)
    {
        next_minutes = first_minutes;
        next_rule = g_steal_pointer (&first_rule);
    }

    if (next_minutes < 0)
        text = g_strdup (_("Add a rule to switch the style at a given time."));
    else
        text = g_strdup_printf (_("Next switch at %02d:%02d: %s"), next_minutes / 60, next_minutes % 60, next_rule);

    gtk_label_set_text (GTK_LABEL (schedule.status_label), text);
    gtk_widget_set_tooltip_text (schedule.status_label, text);

    g_free (text);
    g_free (next_rule);
    g_free (first_rule);
}

static gboolean
schedule_status_timeout (gpointer user_data)
{
    schedule_update_status ();
    return G_SOURCE_CONTINUE;
}

static void
schedule_save (void)
{
    GtkTreeModel *model = GTK_TREE_MODEL (schedule.store);
    GtkTreeIter iter;
    GPtrArray *values[N_SCHEDULE_COLUMNS];

    for (guint c = 0; c < N_SCHEDULE_COLUMNS; c++)
        values[c] = g_ptr_array_new_with_free_func (g_free);

    if (gtk_tree_model_get_iter_first (model, &iter))
    {
        do
        {
            for (guint c = 0; c < N_SCHEDULE_COLUMNS; c++)
            {
                gchar *value;

                gtk_tree_model_get (model, &iter, c, &value, -1);
                g_ptr_array_add (values[c], value != NULL ? value : g_strdup (""));
            }
        } while (gtk_tree_model_iter_next (model, &iter));
    }

    for (guint c = 0; c < N_SCHEDULE_COLUMNS; c++)
    {
        if (values[c]->len == 0)
            xfconf_channel_reset_property (xsettings_channel, schedule_props[c], FALSE);
        else
        {
            g_ptr_array_add (values[c], NULL);
            xfconf_channel_set_string_list (xsettings_channel, schedule_props[c], (const gchar *const *) values[c]->pdata);
        }
        g_ptr_array_unref (values[c]);
    }

    schedule_update_status ();
}

static void
schedule_load (void)
{
    gchar **values[N_SCHEDULE_COLUMNS];
    guint n_values[N_SCHEDULE_COLUMNS];

    for (guint c = 0; c < N_SCHEDULE_COLUMNS; c++)
    {
        values[c] = xfconf_channel_get_string_list (xsettings_channel, schedule_props[c]);
        n_values[c] = values[c] != NULL ? g_strv_length (values[c]) : 0;
    }

    for (guint i = 0; i < n_values[COLUMN_SCHEDULE_TIME]; i++)
    {
        GtkTreeIter iter;

        gtk_list_store_append (schedule.store, &iter);
        for (guint c = 0; c < N_SCHEDULE_COLUMNS; c++)
            gtk_list_store_set (schedule.store, &iter, c, i < n_values[c] ? values[c][i] : "", -1);

        /* rules saved before color schemes were configurable follow the theme name */
        if (i >= n_values[COLUMN_SCHEDULE_COLOR_SCHEME]
            && i < n_values[COLUMN_SCHEDULE_THEME]
            && !xfce_str_is_empty (values[COLUMN_SCHEDULE_THEME][i]) && *values[COLUMN_SCHEDULE_THEME][i] != ':')
            gtk_list_store_set (schedule.store, &iter, COLUMN_SCHEDULE_COLOR_SCHEME,
                                theme_variant_is_dark (values[COLUMN_SCHEDULE_THEME][i]) ? "prefer-dark" : "default",
                                -1);
    }

    for (guint c = 0; c < N_SCHEDULE_COLUMNS; c++)
        g_strfreev (values[c]);
}

/* whether a rule other than the one at @except_path (may be NULL) is at @minutes */
static gboolean
schedule_time_taken (gint minutes,
                     const gchar *except_path)
{
    GtkTreeModel *model = GTK_TREE_MODEL (schedule.store);
    GtkTreeIter iter;
    gboolean taken = FALSE;

    if (gtk_tree_model_get_iter_first (model, &iter))
    {
        do
        {
            gchar *time_str, *path;

            gtk_tree_model_get (model, &iter, COLUMN_SCHEDULE_TIME, &time_str, -1);
            path = gtk_tree_model_get_string_from_iter (model, &iter);
            taken = schedule_parse_time (time_str) == minutes && g_strcmp0 (path, except_path) != 0;
            g_free (time_str);
            g_free (path);
        } while (!taken && gtk_tree_model_iter_next (model, &iter));
    }

    return taken;
}

static void
schedule_set_time (const gchar *path,
                   gint minutes)
{
    GtkTreeIter iter;
    gchar *time_str;

    if (schedule_time_taken (minutes, path))
    {
        gtk_widget_error_bell (schedule.treeview);
        time_str = g_strdup_printf (_("There is already a rule at %02d:%02d."), minutes / 60, minutes % 60);
        gtk_label_set_text (GTK_LABEL (schedule.status_label), time_str);
        g_free (time_str);
        return;
    }

    if (gtk_tree_model_get_iter_from_string (GTK_TREE_MODEL (schedule.store), &iter, path))
    {
        time_str = g_strdup_printf ("%02d:%02d", minutes / 60, minutes % 60);
        gtk_list_store_set (schedule.store, &iter, COLUMN_SCHEDULE_TIME, time_str, -1);
        g_free (time_str);
        schedule_save ();
    }
}

/* two digits also for the hours, like in the list */
static gboolean
cb_schedule_time_spin_output (GtkSpinButton *spin)
{
    gchar *text = g_strdup_printf ("%02d", gtk_spin_button_get_value_as_int (spin));

    gtk_entry_set_text (GTK_ENTRY (spin), text);
    g_free (text);

    return TRUE;
}

static void
cb_schedule_time_popover_closed (GtkPopover *popover)
{
    gchar *time_str, *old_time_str;
    GtkTreeIter iter;

    if (schedule.time_path == NULL)
        return;

    /* take digits typed but not confirmed yet */
    gtk_spin_button_update (GTK_SPIN_BUTTON (schedule.hour_spin));
    gtk_spin_button_update (GTK_SPIN_BUTTON (schedule.minute_spin));

    if (gtk_tree_model_get_iter_from_string (GTK_TREE_MODEL (schedule.store), &iter, schedule.time_path))
    {
        gint minutes = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (schedule.hour_spin)) * 60
                       + gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (schedule.minute_spin));

        gtk_tree_model_get (GTK_TREE_MODEL (schedule.store), &iter, COLUMN_SCHEDULE_TIME, &old_time_str, -1);
        time_str = g_strdup_printf ("%02d:%02d", minutes / 60, minutes % 60);
        if (g_strcmp0 (time_str, old_time_str) != 0)
            schedule_set_time (schedule.time_path, minutes);
        g_free (time_str);
        g_free (old_time_str);
    }

    g_clear_pointer (&schedule.time_path, g_free);
    gtk_widget_grab_focus (schedule.treeview);
}

static GtkWidget *
schedule_time_spin_new (gint max)
{
    GtkWidget *spin = gtk_spin_button_new_with_range (0, max, 1);

    gtk_orientable_set_orientation (GTK_ORIENTABLE (spin), GTK_ORIENTATION_VERTICAL);
    gtk_spin_button_set_wrap (GTK_SPIN_BUTTON (spin), TRUE);
    gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (spin), TRUE);
    gtk_spin_button_set_increments (GTK_SPIN_BUTTON (spin), 1, max == 59 ? 10 : 6);
    gtk_entry_set_width_chars (GTK_ENTRY (spin), 2);
    gtk_entry_set_alignment (GTK_ENTRY (spin), 0.5);
    g_signal_connect (spin, "output", G_CALLBACK (cb_schedule_time_spin_output), NULL);
    g_signal_connect_swapped (spin, "activate", G_CALLBACK (gtk_popover_popdown), schedule.time_popover);

    return spin;
}

/* GTK has no time picker: hours and minutes in a popover at the cell */
static void
schedule_edit_time (GtkTreePath *path)
{
    GtkTreeView *treeview = GTK_TREE_VIEW (schedule.treeview);
    GtkTreeIter iter;
    GdkRectangle rect;
    GtkWidget *box;
    gchar *time_str;
    gint minutes;

    if (!gtk_tree_model_get_iter (GTK_TREE_MODEL (schedule.store), &iter, path))
        return;

    if (schedule.time_popover == NULL)
    {
        schedule.time_popover = gtk_popover_new (schedule.treeview);
        gtk_popover_set_position (GTK_POPOVER (schedule.time_popover), GTK_POS_BOTTOM);
        g_signal_connect (schedule.time_popover, "closed", G_CALLBACK (cb_schedule_time_popover_closed), NULL);

        box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
        gtk_container_set_border_width (GTK_CONTAINER (box), 12);
        gtk_container_add (GTK_CONTAINER (schedule.time_popover), box);

        schedule.hour_spin = schedule_time_spin_new (23);
        gtk_box_pack_start (GTK_BOX (box), schedule.hour_spin, FALSE, FALSE, 0);
        gtk_box_pack_start (GTK_BOX (box), gtk_label_new (":"), FALSE, FALSE, 0);
        schedule.minute_spin = schedule_time_spin_new (59);
        gtk_box_pack_start (GTK_BOX (box), schedule.minute_spin, FALSE, FALSE, 0);

        gtk_widget_show_all (box);
    }

    gtk_tree_model_get (GTK_TREE_MODEL (schedule.store), &iter, COLUMN_SCHEDULE_TIME, &time_str, -1);
    minutes = MAX (schedule_parse_time (time_str), 0);
    g_free (time_str);

    g_free (schedule.time_path);
    schedule.time_path = gtk_tree_path_to_string (path);
    gtk_spin_button_set_value (GTK_SPIN_BUTTON (schedule.hour_spin), minutes / 60);
    gtk_spin_button_set_value (GTK_SPIN_BUTTON (schedule.minute_spin), minutes % 60);

    gtk_tree_view_get_cell_area (treeview, path, schedule.time_column, &rect);
    gtk_tree_view_convert_bin_window_to_widget_coords (treeview, rect.x, rect.y, &rect.x, &rect.y);
    gtk_popover_set_pointing_to (GTK_POPOVER (schedule.time_popover), &rect);
    gtk_popover_popup (GTK_POPOVER (schedule.time_popover));
    gtk_widget_grab_focus (schedule.hour_spin);
}

/* the row has to be laid out to place the popover at it */
static gboolean
schedule_edit_time_idle (gpointer data)
{
    GtkTreePath *path = gtk_tree_path_new_from_string (data);

    schedule_edit_time (path);
    gtk_tree_path_free (path);

    return G_SOURCE_REMOVE;
}

static void
cb_schedule_row_activated (GtkTreeView *treeview,
                           GtkTreePath *path,
                           GtkTreeViewColumn *column)
{
    if (column == schedule.time_column)
        schedule_edit_time (path);
}

static void
cb_schedule_theme_edited (GtkCellRendererText *renderer,
                          gchar *path,
                          gchar *new_text)
{
    GtkTreeIter iter;
    gchar *value;

    value = schedule_choice_value (schedule.theme_choices, new_text);
    if (value != NULL
        && gtk_tree_model_get_iter_from_string (GTK_TREE_MODEL (schedule.store), &iter, path))
    {
        gtk_list_store_set (schedule.store, &iter, COLUMN_SCHEDULE_THEME, value, -1);

        /* a style comes with the matching color scheme */
        if (g_strcmp0 (value, ":dark") == 0
            || (!xfce_str_is_empty (value) && *value != ':' && theme_variant_is_dark (value)))
            gtk_list_store_set (schedule.store, &iter, COLUMN_SCHEDULE_COLOR_SCHEME, "prefer-dark", -1);
        else if (!xfce_str_is_empty (value))
            gtk_list_store_set (schedule.store, &iter, COLUMN_SCHEDULE_COLOR_SCHEME, "prefer-light", -1);

        schedule_save ();
    }
    g_free (value);
}

static void
cb_schedule_color_scheme_edited (GtkCellRendererText *renderer,
                                 gchar *path,
                                 gchar *new_text)
{
    GtkTreeIter iter;
    gchar *value;

    value = schedule_choice_value (schedule.color_scheme_choices, new_text);
    if (value != NULL
        && gtk_tree_model_get_iter_from_string (GTK_TREE_MODEL (schedule.store), &iter, path))
    {
        gtk_list_store_set (schedule.store, &iter, COLUMN_SCHEDULE_COLOR_SCHEME, value, -1);
        schedule_save ();
    }
    g_free (value);
}

/* shows the label of a rule's theme or color scheme value */
static void
schedule_choice_cell_data (GtkTreeViewColumn *column,
                           GtkCellRenderer *renderer,
                           GtkTreeModel *model,
                           GtkTreeIter *iter,
                           gpointer user_data)
{
    gint model_column = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (renderer), "schedule-column"));
    gchar *value, *label;

    gtk_tree_model_get (model, iter, model_column, &value, -1);
    label = schedule_choice_label (user_data, value);
    g_object_set (renderer, "text", label, NULL);
    g_free (label);
    g_free (value);
}

static gboolean
schedule_fill_theme_choices (gpointer user_data)
{
    GtkTreeIter iter;

    schedule.theme_choices_idle_id = 0;

    gtk_list_store_clear (schedule.theme_choices);
    gtk_list_store_insert_with_values (schedule.theme_choices, NULL, -1,
                                       COLUMN_CHOICE_LABEL, _("Dark variant"),
                                       COLUMN_CHOICE_VALUE, ":dark", -1);
    gtk_list_store_insert_with_values (schedule.theme_choices, NULL, -1,
                                       COLUMN_CHOICE_LABEL, _("Light variant"),
                                       COLUMN_CHOICE_VALUE, ":light", -1);
    gtk_list_store_insert_with_values (schedule.theme_choices, NULL, -1,
                                       COLUMN_CHOICE_LABEL, _("No change"),
                                       COLUMN_CHOICE_VALUE, "", -1);

    if (gtk_tree_model_get_iter_first (schedule.themes, &iter))
    {
        do
        {
            gchar *theme;

            gtk_tree_model_get (schedule.themes, &iter, COLUMN_THEME_NAME, &theme, -1);
            if (theme != NULL)
                gtk_list_store_insert_with_values (schedule.theme_choices, NULL, -1,
                                                   COLUMN_CHOICE_LABEL, theme,
                                                   COLUMN_CHOICE_VALUE, theme, -1);
            g_free (theme);
        } while (gtk_tree_model_iter_next (schedule.themes, &iter));
    }

    schedule_update_status ();

    return G_SOURCE_REMOVE;
}

/* the Style tab (re)loads its theme list in idle callbacks */
static void
cb_schedule_themes_changed (void)
{
    if (schedule.theme_choices_idle_id == 0)
        schedule.theme_choices_idle_id = g_idle_add (schedule_fill_theme_choices, NULL);
}

static void
cb_schedule_add_clicked (GtkButton *button)
{
    GtkTreeIter iter;
    GtkTreePath *path;
    GDateTime *now;
    gchar *time_str;
    gint start, minutes = -1;

    /* the first free full hour from the next one on, else the first free minute */
    now = g_date_time_new_now_local ();
    start = ((g_date_time_get_hour (now) + 1) % 24) * 60;
    g_date_time_unref (now);

    for (gint i = 0; i < 24 * 60 && minutes < 0; i += 60)
        if (!schedule_time_taken ((start + i) % (24 * 60), NULL))
            minutes = (start + i) % (24 * 60);
    for (gint i = 0; i < 24 * 60 && minutes < 0; i++)
        if (!schedule_time_taken ((start + i) % (24 * 60), NULL))
            minutes = (start + i) % (24 * 60);

    if (minutes < 0)
    {
        gtk_widget_error_bell (schedule.treeview);
        return;
    }

    /* the typical evening rule: dark variant of the current style, dark colors */
    time_str = g_strdup_printf ("%02d:%02d", minutes / 60, minutes % 60);
    gtk_list_store_insert_with_values (schedule.store, &iter, -1,
                                       COLUMN_SCHEDULE_TIME, time_str,
                                       COLUMN_SCHEDULE_THEME, ":dark",
                                       COLUMN_SCHEDULE_COLOR_SCHEME, "prefer-dark",
                                       COLUMN_SCHEDULE_XFWM4,
                                       xfconf_channel_get_bool (xsettings_channel, "/Xfce/SyncThemes", FALSE) ? "true" : "false",
                                       COLUMN_SCHEDULE_ICON_THEME, "",
                                       COLUMN_SCHEDULE_CURSOR_THEME, "",
                                       COLUMN_SCHEDULE_BRIGHTNESS, "",
                                       COLUMN_SCHEDULE_COMMAND, "",
                                       -1);
    g_free (time_str);

    schedule_save ();

    /* let the user pick the time right away */
    path = gtk_tree_model_get_path (GTK_TREE_MODEL (schedule.store), &iter);
    gtk_widget_grab_focus (schedule.treeview);
    gtk_tree_view_set_cursor (GTK_TREE_VIEW (schedule.treeview), path, schedule.time_column, FALSE);
    g_idle_add_full (G_PRIORITY_DEFAULT_IDLE, schedule_edit_time_idle,
                     gtk_tree_path_to_string (path), g_free);
    gtk_tree_path_free (path);
}

static void
cb_schedule_remove_clicked (GtkButton *button)
{
    GtkTreeSelection *selection;
    GtkTreeIter iter;

    selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (schedule.treeview));
    if (gtk_tree_selection_get_selected (selection, NULL, &iter))
    {
        gtk_list_store_remove (schedule.store, &iter);
        schedule_save ();
    }
}

static void
cb_schedule_selection_changed (GtkTreeSelection *selection,
                               GtkWidget *remove_button)
{
    gtk_widget_set_sensitive (remove_button, gtk_tree_selection_get_selected (selection, NULL, NULL));
}

static void
cb_schedule_enabled_changed (GObject *object,
                             GParamSpec *pspec)
{
    schedule_update_status ();
}

static gint
schedule_compare_names (gconstpointer a,
                        gconstpointer b)
{
    return g_utf8_collate (*(const gchar **) a, *(const gchar **) b);
}

/* the installed cursor themes, sorted */
static GPtrArray *
schedule_list_cursor_themes (void)
{
    GPtrArray *themes = g_ptr_array_new_with_free_func (g_free);
    gchar **dirs;

    xfce_resource_push_path (XFCE_RESOURCE_ICONS, DATADIR G_DIR_SEPARATOR_S "icons");
    dirs = xfce_resource_dirs (XFCE_RESOURCE_ICONS);
    xfce_resource_pop_path (XFCE_RESOURCE_ICONS);

    for (guint i = 0; dirs[i] != NULL; i++)
    {
        GDir *dir = g_dir_open (dirs[i], 0, NULL);
        const gchar *entry;

        if (dir == NULL)
            continue;

        while ((entry = g_dir_read_name (dir)) != NULL)
        {
            gchar *path = g_build_filename (dirs[i], entry, "cursors", NULL);

            if (g_file_test (path, G_FILE_TEST_IS_DIR)
                && !g_ptr_array_find_with_equal_func (themes, entry, g_str_equal, NULL))
                g_ptr_array_add (themes, g_strdup (entry));
            g_free (path);
        }

        g_dir_close (dir);
    }

    g_strfreev (dirs);
    g_ptr_array_sort (themes, schedule_compare_names);

    return themes;
}

/* selects @id in @combo, adding it if it is not listed (e.g. uninstalled) */
static void
schedule_combo_set_active_id (GtkComboBoxText *combo,
                              const gchar *id)
{
    if (!gtk_combo_box_set_active_id (GTK_COMBO_BOX (combo), id))
    {
        gtk_combo_box_text_append (combo, id, id);
        gtk_combo_box_set_active_id (GTK_COMBO_BOX (combo), id);
    }
}

static GtkWidget *
schedule_more_check_combo (GtkGrid *grid,
                           gint row,
                           const gchar *label,
                           GtkWidget **combo)
{
    GtkWidget *check = gtk_check_button_new_with_mnemonic (label);

    *combo = gtk_combo_box_text_new ();
    gtk_widget_set_hexpand (*combo, TRUE);
    g_object_bind_property (check, "active", *combo, "sensitive", G_BINDING_SYNC_CREATE);
    gtk_grid_attach (grid, check, 0, row, 1, 1);
    gtk_grid_attach (grid, *combo, 1, row, 1, 1);

    return check;
}

/* the dialog with the further actions of the rule at @path_str */
static void
schedule_edit_more (const gchar *path_str)
{
    GtkTreeModel *model = GTK_TREE_MODEL (schedule.store);
    GtkTreeIter iter;
    GtkWidget *dialog, *grid, *label, *xfwm4_check, *icon_check, *icon_combo;
    GtkWidget *cursor_check, *cursor_combo, *brightness_check, *brightness_spin, *box, *command_entry;
    gchar *time_str, *theme, *xfwm4, *icon_theme, *cursor_theme, *brightness, *command, *title, *current;
    gchar *backlight;
    GPtrArray *cursor_themes;
    gint row = 0;

    if (!gtk_tree_model_get_iter_from_string (model, &iter, path_str))
        return;

    gtk_tree_model_get (model, &iter,
                        COLUMN_SCHEDULE_TIME, &time_str,
                        COLUMN_SCHEDULE_THEME, &theme,
                        COLUMN_SCHEDULE_XFWM4, &xfwm4,
                        COLUMN_SCHEDULE_ICON_THEME, &icon_theme,
                        COLUMN_SCHEDULE_CURSOR_THEME, &cursor_theme,
                        COLUMN_SCHEDULE_BRIGHTNESS, &brightness,
                        COLUMN_SCHEDULE_COMMAND, &command,
                        -1);

    title = g_strdup_printf (_("Rule at %s"), time_str);
    dialog = gtk_dialog_new_with_buttons (title, GTK_WINDOW (gtk_widget_get_toplevel (schedule.treeview)),
                                          GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                          _("_Cancel"), GTK_RESPONSE_CANCEL, _("_OK"), GTK_RESPONSE_OK, NULL);
    gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);
    g_free (title);

    grid = gtk_grid_new ();
    gtk_grid_set_row_spacing (GTK_GRID (grid), 6);
    gtk_grid_set_column_spacing (GTK_GRID (grid), 12);
    gtk_container_set_border_width (GTK_CONTAINER (grid), 12);
    gtk_box_pack_start (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (dialog))), grid, TRUE, TRUE, 0);

    /* Xfwm4 theme, by default as the switch on the Style tab */
    xfwm4_check = gtk_check_button_new_with_mnemonic (_("Set the matching _window manager theme along with the theme"));
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (xfwm4_check),
                                  !xfce_str_is_empty (xfwm4) ? g_strcmp0 (xfwm4, "true") == 0
                                                             : xfconf_channel_get_bool (xsettings_channel, "/Xfce/SyncThemes", FALSE));
    gtk_grid_attach (GTK_GRID (grid), xfwm4_check, 0, row++, 2, 1);

    /* icon theme, by default the same kind of variant as the theme */
    icon_check = schedule_more_check_combo (GTK_GRID (grid), row++, _("Set the _icon theme:"), &icon_combo);
    gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (icon_combo), ":dark", _("Dark variant"));
    gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (icon_combo), ":light", _("Light variant"));
    if (schedule.icon_themes != NULL && gtk_tree_model_get_iter_first (schedule.icon_themes, &iter))
    {
        do
        {
            gchar *name;

            gtk_tree_model_get (schedule.icon_themes, &iter, COLUMN_THEME_NAME, &name, -1);
            if (name != NULL)
                gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (icon_combo), name, name);
            g_free (name);
        } while (gtk_tree_model_iter_next (schedule.icon_themes, &iter));
    }
    current = xfconf_channel_get_string (xsettings_channel, "/Net/IconThemeName", "Rodent");
    schedule_combo_set_active_id (GTK_COMBO_BOX_TEXT (icon_combo),
                                  !xfce_str_is_empty (icon_theme) ? icon_theme
                                  : *theme == ':'                 ? theme
                                                                  : current);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (icon_check), !xfce_str_is_empty (icon_theme));
    g_free (current);

    /* cursor theme */
    cursor_check = schedule_more_check_combo (GTK_GRID (grid), row++, _("Set the mouse _cursor theme:"), &cursor_combo);
    cursor_themes = schedule_list_cursor_themes ();
    for (guint i = 0; i < cursor_themes->len; i++)
        gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (cursor_combo),
                                   g_ptr_array_index (cursor_themes, i), g_ptr_array_index (cursor_themes, i));
    g_ptr_array_unref (cursor_themes);
    current = xfconf_channel_get_string (xsettings_channel, "/Gtk/CursorThemeName", "default");
    schedule_combo_set_active_id (GTK_COMBO_BOX_TEXT (cursor_combo),
                                  !xfce_str_is_empty (cursor_theme) ? cursor_theme : (!xfce_str_is_empty (current) ? current : "default"));
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (cursor_check), !xfce_str_is_empty (cursor_theme));
    g_free (current);

    /* screen backlight, by default at its current level */
    backlight = backlight_find_device ();
    brightness_check = gtk_check_button_new_with_mnemonic (_("Set the screen _brightness to:"));
    gtk_grid_attach (GTK_GRID (grid), brightness_check, 0, row, 1, 1);
    box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
    brightness_spin = gtk_spin_button_new_with_range (1, 100, 5);
    gtk_spin_button_set_value (GTK_SPIN_BUTTON (brightness_spin),
                               !xfce_str_is_empty (brightness) ? g_ascii_strtod (brightness, NULL)
                               : backlight != NULL             ? backlight_get_percent (backlight)
                                                               : 100);
    gtk_entry_set_activates_default (GTK_ENTRY (brightness_spin), TRUE);
    gtk_box_pack_start (GTK_BOX (box), brightness_spin, FALSE, FALSE, 0);
    gtk_box_pack_start (GTK_BOX (box), gtk_label_new ("%"), FALSE, FALSE, 0);
    g_object_bind_property (brightness_check, "active", box, "sensitive", G_BINDING_SYNC_CREATE);
    gtk_grid_attach (GTK_GRID (grid), box, 1, row++, 1, 1);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (brightness_check), !xfce_str_is_empty (brightness));
    if (backlight == NULL)
    {
        /* e.g. external monitors: keep a saved value, but do not offer it */
        gtk_widget_set_sensitive (brightness_check, !xfce_str_is_empty (brightness));
        gtk_widget_set_tooltip_text (brightness_check, _("No screen backlight was found."));
    }
    g_free (backlight);

    /* shell command */
    label = gtk_label_new_with_mnemonic (_("Run co_mmand:"));
    gtk_label_set_xalign (GTK_LABEL (label), 0.0);
    gtk_grid_attach (GTK_GRID (grid), label, 0, row, 1, 1);
    command_entry = gtk_entry_new ();
    gtk_entry_set_text (GTK_ENTRY (command_entry), command);
    gtk_entry_set_placeholder_text (GTK_ENTRY (command_entry), "notify-send \"$XFCE_SCHEDULE_GTK_THEME\"");
    gtk_entry_set_activates_default (GTK_ENTRY (command_entry), TRUE);
    gtk_widget_set_hexpand (command_entry, TRUE);
    gtk_label_set_mnemonic_widget (GTK_LABEL (label), command_entry);
    gtk_grid_attach (GTK_GRID (grid), command_entry, 1, row++, 1, 1);

    label = gtk_label_new (_("The command runs with /bin/sh after switching. The new settings are in "
                             "$XFCE_SCHEDULE_GTK_THEME, $XFCE_SCHEDULE_ICON_THEME and $XFCE_SCHEDULE_COLOR_SCHEME."));
    gtk_label_set_xalign (GTK_LABEL (label), 0.0);
    gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
    gtk_label_set_max_width_chars (GTK_LABEL (label), 50);
    gtk_style_context_add_class (gtk_widget_get_style_context (label), GTK_STYLE_CLASS_DIM_LABEL);
    gtk_grid_attach (GTK_GRID (grid), label, 0, row++, 2, 1);

    gtk_widget_show_all (grid);

    if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_OK
        && gtk_tree_model_get_iter_from_string (model, &iter, path_str))
    {
        gchar *new_command = g_strstrip (g_strdup (gtk_entry_get_text (GTK_ENTRY (command_entry))));
        gboolean icons = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (icon_check));
        gboolean cursors = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (cursor_check));
        gchar *new_brightness = NULL;

        if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (brightness_check)))
        {
            gtk_spin_button_update (GTK_SPIN_BUTTON (brightness_spin));
            new_brightness = g_strdup_printf ("%d", gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (brightness_spin)));
        }

        gtk_list_store_set (schedule.store, &iter,
                            COLUMN_SCHEDULE_XFWM4,
                            gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (xfwm4_check)) ? "true" : "false",
                            COLUMN_SCHEDULE_ICON_THEME,
                            icons ? gtk_combo_box_get_active_id (GTK_COMBO_BOX (icon_combo)) : "",
                            COLUMN_SCHEDULE_CURSOR_THEME,
                            cursors ? gtk_combo_box_get_active_id (GTK_COMBO_BOX (cursor_combo)) : "",
                            COLUMN_SCHEDULE_BRIGHTNESS, new_brightness != NULL ? new_brightness : "",
                            COLUMN_SCHEDULE_COMMAND, new_command,
                            -1);
        g_free (new_brightness);
        g_free (new_command);
        schedule_save ();
    }

    gtk_widget_destroy (dialog);

    g_free (time_str);
    g_free (theme);
    g_free (xfwm4);
    g_free (icon_theme);
    g_free (cursor_theme);
    g_free (brightness);
    g_free (command);
}

static gboolean
schedule_edit_more_idle (gpointer user_data)
{
    schedule_edit_more (user_data);
    return G_SOURCE_REMOVE;
}

/* a short list of the further actions of a rule */
static void
schedule_more_cell_data (GtkTreeViewColumn *column,
                         GtkCellRenderer *renderer,
                         GtkTreeModel *model,
                         GtkTreeIter *iter,
                         gpointer user_data)
{
    GPtrArray *parts = g_ptr_array_new_with_free_func (g_free);
    gchar *icon_theme, *cursor_theme, *brightness, *command, *text;

    gtk_tree_model_get (model, iter,
                        COLUMN_SCHEDULE_ICON_THEME, &icon_theme,
                        COLUMN_SCHEDULE_CURSOR_THEME, &cursor_theme,
                        COLUMN_SCHEDULE_BRIGHTNESS, &brightness,
                        COLUMN_SCHEDULE_COMMAND, &command,
                        -1);

    if (!xfce_str_is_empty (icon_theme))
        g_ptr_array_add (parts, g_strdup (_("icons")));
    if (!xfce_str_is_empty (cursor_theme))
        g_ptr_array_add (parts, g_strdup (_("cursor")));
    if (!xfce_str_is_empty (brightness))
        /* TRANSLATORS: the screen brightness set by a rule, in the More column, e.g. "brightness 40%" */
        g_ptr_array_add (parts, g_strdup_printf (_("brightness %s%%"), brightness));
    if (!xfce_str_is_empty (command))
        g_ptr_array_add (parts, g_strdup (_("command")));
    g_ptr_array_add (parts, NULL);

    text = g_strjoinv (", ", (gchar **) parts->pdata);
    g_object_set (renderer, "text", text, NULL);

    g_free (text);
    g_ptr_array_unref (parts);
    g_free (icon_theme);
    g_free (cursor_theme);
    g_free (brightness);
    g_free (command);
}

/* a click into the Time column opens the time picker, into the More column
 * the dialog of that rule */
static gboolean
cb_schedule_treeview_button_press (GtkWidget *treeview,
                                   GdkEventButton *event)
{
    GtkTreePath *path;
    GtkTreeViewColumn *column;

    if (event->type != GDK_BUTTON_PRESS || event->button != 1
        || event->window != gtk_tree_view_get_bin_window (GTK_TREE_VIEW (treeview))
        || !gtk_tree_view_get_path_at_pos (GTK_TREE_VIEW (treeview), event->x, event->y, &path, &column, NULL, NULL))
        return FALSE;

    if (column == schedule.time_column)
    {
        gtk_widget_grab_focus (treeview);
        gtk_tree_view_set_cursor (GTK_TREE_VIEW (treeview), path, column, FALSE);
        g_idle_add_full (G_PRIORITY_DEFAULT_IDLE, schedule_edit_time_idle,
                         gtk_tree_path_to_string (path), g_free);
    }
    else if (column == schedule.more_column)
    {
        gtk_tree_view_set_cursor (GTK_TREE_VIEW (treeview), path, NULL, FALSE);
        g_idle_add_full (G_PRIORITY_DEFAULT_IDLE, schedule_edit_more_idle,
                         gtk_tree_path_to_string (path), g_free);
    }

    gtk_tree_path_free (path);

    return column == schedule.time_column || column == schedule.more_column;
}

static void
cb_schedule_more_clicked (GtkButton *button)
{
    GtkTreeSelection *selection;
    GtkTreeIter iter;

    selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (schedule.treeview));
    if (gtk_tree_selection_get_selected (selection, NULL, &iter))
    {
        gchar *path_str = gtk_tree_model_get_string_from_iter (GTK_TREE_MODEL (schedule.store), &iter);
        schedule_edit_more (path_str);
        g_free (path_str);
    }
}

static void
cb_schedule_help_clicked (GtkButton *button)
{
    GtkWidget *dialog;
    GString *text;

    dialog = gtk_message_dialog_new (GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (button))),
                                     GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                     GTK_MESSAGE_INFO, GTK_BUTTONS_CLOSE,
                                     _("Switching the style at set times"));
    text = g_string_new (NULL);
    g_string_append (text,
                     _("Each rule takes effect when the clock reaches its time; the settings stay until the next rule. "
                       "A setting changed by hand stays until the next rule as well. A switch missed while you were "
                       "logged out is made at login."));
    g_string_append (text, "\n\n");
    g_string_append (text,
                     _("Theme: \"Dark variant\" and \"Light variant\" are looked up from the theme in use at the time of "
                       "the switch, e.g. Greybird becomes Greybird-dark, Orchis-Compact becomes Orchis-Dark-Compact. "
                       "Upper and lower case do not matter. If there is no such variant, the theme stays. "
                       "The theme is used by Xfce and GTK 3 applications."));
    g_string_append (text, "\n\n");
    g_string_append (text,
                     _("Color scheme: the dark or light preference of the desktop, read by GTK 4 and libadwaita, "
                       "Qt 6, Firefox, Chromium and Electron applications. Firefox follows it if Website appearance "
                       "is set to Automatic, Chromium if Mode is set to Device."));
    g_string_append (text, "\n\n");
    g_string_append (text,
                     _("More: also set the window manager theme, the icon theme, the mouse cursor theme or the "
                       "brightness of a laptop screen, or run a command of your own."));
    gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog), "%s", text->str);
    g_string_free (text, TRUE);
    gtk_dialog_run (GTK_DIALOG (dialog));
    gtk_widget_destroy (dialog);
}

static void
cb_schedule_current_theme_changed (XfconfChannel *channel,
                                   const gchar *property,
                                   const GValue *value)
{
    schedule_update_status ();
}

static void
appearance_settings_dialog_configure_schedule (GtkBuilder *builder)
{
    GtkWidget *page, *box, *label, *toggle, *rules_box, *scrolled, *button_box, *button;
    GtkCellRenderer *renderer;
    GtkTreeViewColumn *column;
    GtkTreeSelection *selection;
    GObject *notebook;

    schedule.themes = gtk_tree_view_get_model (GTK_TREE_VIEW (gtk_builder_get_object (builder, "gtk_theme_treeview")));
    GType column_types[N_SCHEDULE_COLUMNS];

    for (guint c = 0; c < N_SCHEDULE_COLUMNS; c++)
        column_types[c] = G_TYPE_STRING;

    schedule.icon_themes = gtk_tree_view_get_model (GTK_TREE_VIEW (gtk_builder_get_object (builder, "icon_theme_treeview")));
    schedule.store = gtk_list_store_newv (N_SCHEDULE_COLUMNS, column_types);
    gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (schedule.store), COLUMN_SCHEDULE_TIME, GTK_SORT_ASCENDING);
    schedule_load ();

    schedule.theme_choices = gtk_list_store_new (N_CHOICE_COLUMNS, G_TYPE_STRING, G_TYPE_STRING);
    schedule_fill_theme_choices (NULL);
    g_signal_connect (schedule.themes, "row-inserted", G_CALLBACK (cb_schedule_themes_changed), NULL);
    g_signal_connect (schedule.themes, "row-changed", G_CALLBACK (cb_schedule_themes_changed), NULL);
    g_signal_connect (schedule.themes, "row-deleted", G_CALLBACK (cb_schedule_themes_changed), NULL);

    schedule.color_scheme_choices = gtk_list_store_new (N_CHOICE_COLUMNS, G_TYPE_STRING, G_TYPE_STRING);
    gtk_list_store_insert_with_values (schedule.color_scheme_choices, NULL, -1,
                                       COLUMN_CHOICE_LABEL, _("Dark"),
                                       COLUMN_CHOICE_VALUE, "prefer-dark", -1);
    gtk_list_store_insert_with_values (schedule.color_scheme_choices, NULL, -1,
                                       COLUMN_CHOICE_LABEL, _("Light"),
                                       COLUMN_CHOICE_VALUE, "prefer-light", -1);
    gtk_list_store_insert_with_values (schedule.color_scheme_choices, NULL, -1,
                                       COLUMN_CHOICE_LABEL, _("Default"),
                                       COLUMN_CHOICE_VALUE, "default", -1);
    gtk_list_store_insert_with_values (schedule.color_scheme_choices, NULL, -1,
                                       COLUMN_CHOICE_LABEL, _("No change"),
                                       COLUMN_CHOICE_VALUE, "", -1);

    page = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
    gtk_container_set_border_width (GTK_CONTAINER (page), 12);

    /* Enable switch */
    box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
    label = gtk_label_new_with_mnemonic (_("Change the style automatica_lly at set times"));
    gtk_label_set_xalign (GTK_LABEL (label), 0.0);
    gtk_widget_set_hexpand (label, TRUE);
    gtk_box_pack_start (GTK_BOX (box), label, TRUE, TRUE, 0);
    toggle = gtk_switch_new ();
    gtk_widget_set_valign (toggle, GTK_ALIGN_CENTER);
    gtk_label_set_mnemonic_widget (GTK_LABEL (label), toggle);
    gtk_box_pack_end (GTK_BOX (box), toggle, FALSE, FALSE, 0);
    gtk_box_pack_start (GTK_BOX (page), box, FALSE, FALSE, 0);

    xfconf_g_property_bind (xsettings_channel, SCHEDULE_ENABLED_PROP, G_TYPE_BOOLEAN, G_OBJECT (toggle), "active");
    g_signal_connect (toggle, "notify::active", G_CALLBACK (cb_schedule_enabled_changed), NULL);

    /* Rules */
    rules_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
    g_object_bind_property (toggle, "active", rules_box, "sensitive", G_BINDING_SYNC_CREATE);
    gtk_box_pack_start (GTK_BOX (page), rules_box, TRUE, TRUE, 0);

    scrolled = gtk_scrolled_window_new (NULL, NULL);
    gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolled), GTK_SHADOW_ETCHED_IN);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_vexpand (scrolled, TRUE);
    gtk_box_pack_start (GTK_BOX (rules_box), scrolled, TRUE, TRUE, 0);

    schedule.treeview = gtk_tree_view_new_with_model (GTK_TREE_MODEL (schedule.store));
    g_object_unref (schedule.store);
    gtk_container_add (GTK_CONTAINER (scrolled), schedule.treeview);

    renderer = gtk_cell_renderer_text_new ();
    schedule.time_column = gtk_tree_view_column_new_with_attributes (_("Time"), renderer,
                                                                     "text", COLUMN_SCHEDULE_TIME, NULL);
    gtk_tree_view_append_column (GTK_TREE_VIEW (schedule.treeview), schedule.time_column);

    renderer = gtk_cell_renderer_combo_new ();
    g_object_set (renderer,
                  "model", schedule.theme_choices,
                  "text-column", COLUMN_CHOICE_LABEL,
                  "has-entry", FALSE,
                  "editable", TRUE,
                  "ellipsize", PANGO_ELLIPSIZE_END,
                  NULL);
    g_object_set_data (G_OBJECT (renderer), "schedule-column", GINT_TO_POINTER (COLUMN_SCHEDULE_THEME));
    g_signal_connect (renderer, "edited", G_CALLBACK (cb_schedule_theme_edited), NULL);
    column = gtk_tree_view_column_new_with_attributes (_("Theme"), renderer, NULL);
    gtk_tree_view_column_set_cell_data_func (column, renderer, schedule_choice_cell_data, schedule.theme_choices, NULL);
    gtk_tree_view_column_set_expand (column, TRUE);
    gtk_tree_view_append_column (GTK_TREE_VIEW (schedule.treeview), column);

    renderer = gtk_cell_renderer_combo_new ();
    g_object_set (renderer,
                  "model", schedule.color_scheme_choices,
                  "text-column", COLUMN_CHOICE_LABEL,
                  "has-entry", FALSE,
                  "editable", TRUE,
                  NULL);
    g_object_set_data (G_OBJECT (renderer), "schedule-column", GINT_TO_POINTER (COLUMN_SCHEDULE_COLOR_SCHEME));
    g_signal_connect (renderer, "edited", G_CALLBACK (cb_schedule_color_scheme_edited), NULL);
    column = gtk_tree_view_column_new_with_attributes (_("Color scheme"), renderer, NULL);
    gtk_tree_view_column_set_cell_data_func (column, renderer, schedule_choice_cell_data, schedule.color_scheme_choices, NULL);
    gtk_tree_view_append_column (GTK_TREE_VIEW (schedule.treeview), column);

    schedule.more_column = gtk_tree_view_column_new ();
    gtk_tree_view_column_set_title (schedule.more_column, _("More"));
    renderer = gtk_cell_renderer_pixbuf_new ();
    g_object_set (renderer, "icon-name", "view-more-symbolic", NULL);
    gtk_tree_view_column_pack_start (schedule.more_column, renderer, FALSE);
    renderer = gtk_cell_renderer_text_new ();
    g_object_set (renderer, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
    gtk_tree_view_column_pack_start (schedule.more_column, renderer, TRUE);
    gtk_tree_view_column_set_cell_data_func (schedule.more_column, renderer, schedule_more_cell_data, NULL, NULL);
    gtk_tree_view_append_column (GTK_TREE_VIEW (schedule.treeview), schedule.more_column);
    g_signal_connect (schedule.treeview, "button-press-event", G_CALLBACK (cb_schedule_treeview_button_press), NULL);
    g_signal_connect (schedule.treeview, "row-activated", G_CALLBACK (cb_schedule_row_activated), NULL);

    /* Add and remove buttons */
    button_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_box_pack_start (GTK_BOX (rules_box), button_box, FALSE, FALSE, 0);

    button = gtk_button_new_with_mnemonic (_("_Add"));
    gtk_button_set_image (GTK_BUTTON (button), gtk_image_new_from_icon_name ("list-add", GTK_ICON_SIZE_BUTTON));
    gtk_button_set_always_show_image (GTK_BUTTON (button), TRUE);
    g_signal_connect (button, "clicked", G_CALLBACK (cb_schedule_add_clicked), NULL);
    gtk_box_pack_start (GTK_BOX (button_box), button, FALSE, FALSE, 0);

    button = gtk_button_new_with_mnemonic (_("_Remove"));
    gtk_button_set_image (GTK_BUTTON (button), gtk_image_new_from_icon_name ("list-remove", GTK_ICON_SIZE_BUTTON));
    gtk_button_set_always_show_image (GTK_BUTTON (button), TRUE);
    gtk_widget_set_sensitive (button, FALSE);
    g_signal_connect (button, "clicked", G_CALLBACK (cb_schedule_remove_clicked), NULL);
    gtk_box_pack_start (GTK_BOX (button_box), button, FALSE, FALSE, 0);

    selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (schedule.treeview));
    gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);
    g_signal_connect (selection, "changed", G_CALLBACK (cb_schedule_selection_changed), button);

    button = gtk_button_new_with_mnemonic (_("_More…"));
    gtk_widget_set_sensitive (button, FALSE);
    g_signal_connect (button, "clicked", G_CALLBACK (cb_schedule_more_clicked), NULL);
    gtk_box_pack_start (GTK_BOX (button_box), button, FALSE, FALSE, 0);
    g_signal_connect (selection, "changed", G_CALLBACK (cb_schedule_selection_changed), button);

    /* on a row of its own, next to the buttons it gets cut off */
    schedule.status_label = gtk_label_new (NULL);
    gtk_label_set_xalign (GTK_LABEL (schedule.status_label), 0.0);
    gtk_label_set_ellipsize (GTK_LABEL (schedule.status_label), PANGO_ELLIPSIZE_END);
    gtk_box_pack_start (GTK_BOX (rules_box), schedule.status_label, FALSE, FALSE, 0);

    /* Explanation, with a button for the longer help next to it; a "Help"
     * button would be confused with the one of the dialog */
    box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_box_pack_start (GTK_BOX (page), box, FALSE, FALSE, 0);

    label = gtk_label_new (_("The theme is used by Xfce and GTK 3 applications. The color scheme is followed by "
                             "GTK 4 and Qt 6 applications, Firefox, Chromium and other web browsers."));
    gtk_label_set_xalign (GTK_LABEL (label), 0.0);
    gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
    gtk_label_set_max_width_chars (GTK_LABEL (label), 60);
    gtk_style_context_add_class (gtk_widget_get_style_context (label), GTK_STYLE_CLASS_DIM_LABEL);
    gtk_box_pack_start (GTK_BOX (box), label, TRUE, TRUE, 0);

    button = gtk_button_new_from_icon_name ("help-browser", GTK_ICON_SIZE_BUTTON);
    gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NONE);
    gtk_widget_set_valign (button, GTK_ALIGN_CENTER);
    gtk_widget_set_tooltip_text (button, _("Switching the style at set times"));
    g_signal_connect (button, "clicked", G_CALLBACK (cb_schedule_help_clicked), NULL);
    gtk_box_pack_end (GTK_BOX (box), button, FALSE, FALSE, 0);

    notebook = gtk_builder_get_object (builder, "plug-child");
    gtk_widget_show_all (page);
    gtk_notebook_append_page (GTK_NOTEBOOK (notebook), page, gtk_label_new_with_mnemonic (_("_Schedule")));

    /* the variants in the status line depend on the current theme */
    g_signal_connect (xsettings_channel, "property-changed::/Net/ThemeName",
                      G_CALLBACK (cb_schedule_current_theme_changed), NULL);

    schedule_update_status ();
    g_timeout_add_seconds (30, schedule_status_timeout, NULL);
}

static void
appearance_settings_dialog_response (GtkWidget *dialog,
                                     gint response_id)
{
    if (response_id == GTK_RESPONSE_HELP)
        xfce_dialog_show_help_with_version (GTK_WINDOW (dialog), "xfce4-settings", "appearance",
                                            NULL, VERSION_SHORT);
    else
        gtk_main_quit ();
}

gint
main (gint argc,
      gchar **argv)
{
    GObject *dialog;
    GtkBuilder *builder;
    GError *error = NULL;

    /* setup translation domain */
    xfce_textdomain (GETTEXT_PACKAGE, LOCALEDIR, "UTF-8");

    /* initialize Gtk+ */
    if (!gtk_init_with_args (&argc, &argv, NULL, option_entries, GETTEXT_PACKAGE, &error))
    {
        if (G_LIKELY (error))
        {
            /* print error */
            g_print ("%s: %s.\n", G_LOG_DOMAIN, error->message);
            g_print (_("Type '%s --help' for usage."), G_LOG_DOMAIN);
            g_print ("\n");

            /* cleanup */
            g_error_free (error);
        }
        else
        {
            g_critical ("Unable to open display.");
        }

        return EXIT_FAILURE;
    }

    /* print version information */
    if (G_UNLIKELY (opt_version))
    {
        g_print ("%s %s (Xfce %s)\n\n", G_LOG_DOMAIN, VERSION_FULL, xfce_version_string ());
        g_print ("%s\n", "Copyright (c) 2008-" COPYRIGHT_YEAR);
        g_print ("\t%s\n\n", _("The Xfce development team. All rights reserved."));
        g_print (_("Please report bugs to <%s>."), PACKAGE_BUGREPORT);
        g_print ("\n");

        return EXIT_SUCCESS;
    }

    /* initialize xfconf */
    if (!xfconf_init (&error))
    {
        /* print error and exit */
        g_critical ("Failed to connect to xfconf daemon: %s.", error->message);
        g_error_free (error);

        return EXIT_FAILURE;
    }

    /* open the xsettings channel */
    xsettings_channel = xfconf_channel_new ("xsettings");
    if (G_LIKELY (xsettings_channel))
    {
        GSettingsSchemaSource *source;

        /* hook to make sure the libxfce4ui library is linked */
        if (xfce_titled_dialog_get_type () == 0)
            return EXIT_FAILURE;

        /* to synchronize some properties with GSettings */
        source = g_settings_schema_source_get_default ();
        if (source != NULL)
        {
            GSettingsSchema *schema = g_settings_schema_source_lookup (source, "org.gnome.desktop.interface", TRUE);
            if (schema != NULL)
            {
                desktop_interface_gsettings = g_settings_new ("org.gnome.desktop.interface");
                g_settings_schema_unref (schema);
            }
            schema = g_settings_schema_source_lookup (source, "org.x.apps.portal", TRUE);
            if (schema != NULL)
            {
                xapps_portal_gsettings = g_settings_new ("org.x.apps.portal");
                g_settings_schema_unref (schema);
            }
        }

        /* load the gtk user interface file*/
        builder = gtk_builder_new ();
        if (gtk_builder_add_from_resource (builder, "/org/xfce/settings/appearance-dialog.glade", &error) != 0)
        {
            /* connect signal to monitor the channel */
            g_signal_connect (G_OBJECT (xsettings_channel), "property-changed",
                              G_CALLBACK (appearance_settings_dialog_channel_property_changed), builder);

            appearance_settings_dialog_configure_widgets (builder);
            appearance_settings_dialog_configure_schedule (builder);

#ifdef ENABLE_X11
            if (opt_socket_id != 0 && GDK_IS_X11_DISPLAY (gdk_display_get_default ()))
            {
                GtkWidget *plug;
                GObject *plug_child;

                /* Create plug widget */
                plug = gtk_plug_new (opt_socket_id);
                g_signal_connect (plug, "delete-event", G_CALLBACK (gtk_main_quit), NULL);
                gtk_widget_show (plug);

                /* Stop startup notification */
                gdk_notify_startup_complete ();

                /* Get plug child widget */
                plug_child = gtk_builder_get_object (builder, "plug-child");
                xfce_widget_reparent (GTK_WIDGET (plug_child), plug);
                gtk_widget_show (GTK_WIDGET (plug_child));

                /* To prevent the settings dialog to be saved in the session */
                gdk_x11_set_sm_client_id ("FAKE ID");

                /* Enter main loop */
                gtk_main ();
            }
            else
#endif
            {
                /* build the dialog */
                dialog = gtk_builder_get_object (builder, "dialog");

                g_signal_connect (dialog, "response",
                                  G_CALLBACK (appearance_settings_dialog_response), NULL);
                gtk_window_present (GTK_WINDOW (dialog));
#ifdef ENABLE_X11
                /* To prevent the settings dialog to be saved in the session */
                if (GDK_IS_X11_DISPLAY (gdk_display_get_default ()))
                    gdk_x11_set_sm_client_id ("FAKE ID");
#endif
                gtk_main ();
            }
        }
        else
        {
            g_critical ("Failed to load the UI file: %s.", error->message);
            g_error_free (error);
        }

        /* Release Builder */
        g_object_unref (G_OBJECT (builder));

        /* release the channel */
        g_object_unref (G_OBJECT (xsettings_channel));
        if (desktop_interface_gsettings != NULL)
            g_object_unref (desktop_interface_gsettings);
        if (xapps_portal_gsettings != NULL)
            g_object_unref (xapps_portal_gsettings);
    }

    /* shutdown xfconf */
    xfconf_shutdown ();

    return EXIT_SUCCESS;
}
