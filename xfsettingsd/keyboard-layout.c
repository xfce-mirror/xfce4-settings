/*
 *  Copyright (c) 2008 Olivier Fourdan <olivier@xfce.org>
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
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <X11/Xlib.h>
#include <X11/XKBlib.h>

#include <glib.h>
#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <xfconf/xfconf.h>
#include <libxfce4util/libxfce4util.h>

#ifdef HAVE_LIBXKLAVIER
#include <libxklavier/xklavier.h>
#endif /* HAVE_LIBXKLAVIER */

#include "debug.h"
#include "keyboard-layout.h"

static void xfce_keyboard_layout_helper_finalize                  (GObject                       *object);
static void xfce_keyboard_layout_helper_process_xmodmap           (void);
static void xfce_keyboard_layout_helper_set_model                 (XfceKeyboardLayoutHelper      *helper);
static void xfce_keyboard_layout_helper_set_layout                (XfceKeyboardLayoutHelper      *helper);
static void xfce_keyboard_layout_helper_set_variant               (XfceKeyboardLayoutHelper      *helper);
static void xfce_keyboard_layout_helper_set_grpkey                (XfceKeyboardLayoutHelper      *helper);
static void xfce_keyboard_layout_helper_channel_property_changed  (XfconfChannel                 *channel,
                                                                   const gchar                   *property_name,
                                                                   const GValue                  *value,
                                                                   XfceKeyboardLayoutHelper      *helper);
static gchar* xfce_keyboard_layout_get_option                     (gchar                        **options,
                                                                   gchar                         *option_name,
                                                                   gchar                        **other_options);
static GdkFilterReturn handle_xevent                              (GdkXEvent                     *xev,
                                                                   GdkEvent                      *event,
                                                                   XfceKeyboardLayoutHelper      *helper);
static void xfce_keyboard_layout_reset_xkl_config                 (XklEngine                     *xklengine,
                                                                   XfceKeyboardLayoutHelper      *helper);

struct _XfceKeyboardLayoutHelperClass
{
    GObjectClass __parent__;
};

struct _XfceKeyboardLayoutHelper
{
    GObject  __parent__;

    /* xfconf channel */
    XfconfChannel     *channel;

    gboolean           xkb_disable_settings;

#ifdef HAVE_LIBXKLAVIER
    /* libxklavier */
    XklEngine         *engine;
    XklConfigRegistry *registry;
    XklConfigRec      *config;
#endif /* HAVE_LIBXKLAVIER */
};

G_DEFINE_TYPE (XfceKeyboardLayoutHelper, xfce_keyboard_layout_helper, G_TYPE_OBJECT);

static void
xfce_keyboard_layout_helper_class_init (XfceKeyboardLayoutHelperClass *klass)
{
    GObjectClass *gobject_class;

    gobject_class = G_OBJECT_CLASS (klass);
    gobject_class->finalize = xfce_keyboard_layout_helper_finalize;
}

static void
xfce_keyboard_layout_helper_init (XfceKeyboardLayoutHelper *helper)
{
    /* init */
    helper->channel = NULL;

    /* open the channel */
    helper->channel = xfconf_channel_get ("keyboard-layout");

    /* monitor channel changes */
    g_signal_connect (G_OBJECT (helper->channel), "property-changed", G_CALLBACK (xfce_keyboard_layout_helper_channel_property_changed), helper);

#ifdef HAVE_LIBXKLAVIER
    helper->engine = xkl_engine_get_instance (GDK_DISPLAY ());
    helper->config = xkl_config_rec_new ();
    xkl_config_rec_get_from_server (helper->config, helper->engine);

    gdk_window_add_filter (NULL, (GdkFilterFunc) handle_xevent, helper);
    g_signal_connect (helper->engine, "X-new-device",
                      G_CALLBACK (xfce_keyboard_layout_reset_xkl_config), helper);
    xkl_engine_start_listen (helper->engine, XKLL_TRACK_KEYBOARD_STATE);
#endif /* HAVE_LIBXKLAVIER */

    /* load settings */
    helper->xkb_disable_settings = xfconf_channel_get_bool (helper->channel, "/Default/XkbDisable", TRUE);
    xfce_keyboard_layout_helper_set_model (helper);
    xfce_keyboard_layout_helper_set_layout (helper);
    xfce_keyboard_layout_helper_set_variant (helper);
    xfce_keyboard_layout_helper_set_grpkey (helper);

    xfce_keyboard_layout_helper_process_xmodmap ();
}

static void
xfce_keyboard_layout_helper_finalize (GObject *object)
{
    XfceKeyboardLayoutHelper *helper = XFCE_KEYBOARD_LAYOUT_HELPER (object);

#ifdef HAVE_LIBXKLAVIER
    xkl_engine_stop_listen (helper->engine, XKLL_TRACK_KEYBOARD_STATE);
    gdk_window_remove_filter (NULL, (GdkFilterFunc) handle_xevent, helper);
    g_object_unref (helper->config);
    g_object_unref (helper->engine);
#endif /* HAVE_LIBXKLAVIER */

    G_OBJECT_CLASS (xfce_keyboard_layout_helper_parent_class)->finalize (object);
}


static void
xfce_keyboard_layout_helper_process_xmodmap (void)
{
    const gchar *xmodmap_path;

    xmodmap_path = g_build_filename (xfce_get_homedir (), ".Xmodmap", NULL);

    if (g_file_test (xmodmap_path, G_FILE_TEST_EXISTS))
    {
        /* There is a .Xmodmap file, try to use it */
        const gchar *xmodmap_command;
        GError      *error = NULL;

        xmodmap_command = g_strconcat ("xmodmap ", xmodmap_path, NULL);

        xfsettings_dbg (XFSD_DEBUG_KEYBOARD_LAYOUT, "spawning \"%s\"", xmodmap_command);

        /* Launch the xmodmap command and only print errors when in debugging mode */
        if (!g_spawn_command_line_async (xmodmap_command, &error))
        {
            DBG ("Xmodmap call failed: %s", error->message);
            g_error_free (error);
        }
    }

    g_free ((gchar*) xmodmap_path);
}

static void
xfce_keyboard_layout_helper_set_model (XfceKeyboardLayoutHelper *helper)
{
#ifdef HAVE_LIBXKLAVIER
    gchar *xkbmodel;

    if (!helper->xkb_disable_settings)
    {
        xkbmodel = xfconf_channel_get_string (helper->channel, "/Default/XkbModel", helper->config->model);
        g_free (helper->config->model);
        helper->config->model = xkbmodel;
        xkl_config_rec_activate (helper->config, helper->engine);

        xfsettings_dbg (XFSD_DEBUG_KEYBOARD_LAYOUT, "set model to \"%s\"", xkbmodel);
    }
#endif /* HAVE_LIBXKLAVIER */
}

static void
xfce_keyboard_layout_helper_set_layout (XfceKeyboardLayoutHelper *helper)
{
#ifdef HAVE_LIBXKLAVIER
    gchar *default_layouts, *val_layout;
    gchar **layouts;

    if (!helper->xkb_disable_settings)
    {
        default_layouts  = g_strjoinv(",", helper->config->layouts);
        val_layout  = xfconf_channel_get_string (helper->channel, "/Default/XkbLayout",  default_layouts);
        layouts = g_strsplit_set (val_layout, ",", 0);
        g_strfreev(helper->config->layouts);
        helper->config->layouts = layouts;
        xkl_config_rec_activate (helper->config, helper->engine);
        g_free (default_layouts);

        xfsettings_dbg (XFSD_DEBUG_KEYBOARD_LAYOUT, "set layouts to \"%s\"", val_layout);
        g_free (val_layout);
    }
#endif /* HAVE_LIBXKLAVIER */
}

static void
xfce_keyboard_layout_helper_set_variant (XfceKeyboardLayoutHelper *helper)
{
#ifdef HAVE_LIBXKLAVIER
    gchar *default_variants, *val_variant;
    gchar **variants;

    if (!helper->xkb_disable_settings)
    {
        default_variants  = g_strjoinv(",", helper->config->variants);
        val_variant  = xfconf_channel_get_string (helper->channel, "/Default/XkbVariant",  default_variants);
        variants = g_strsplit_set (val_variant, ",", 0);
        g_strfreev(helper->config->variants);
        helper->config->variants = variants;
        xkl_config_rec_activate (helper->config, helper->engine);
        g_free (default_variants);

        xfsettings_dbg (XFSD_DEBUG_KEYBOARD_LAYOUT, "set variant to \"%s\"", val_variant);
        g_free (val_variant);
    }
#endif /* HAVE_LIBXKLAVIER */
}

/**
 * @options - Xkl config options (array of strings terminated in NULL)
 * @option_name the name of the xkb option to look for (e.g., "grp:")
 * @_other_options if not NULL, will be set to the input option string
 *                 excluding @option_name. Needs to be freed with g_free().
 * @return the string in @options array corresponding to @option_name,
 *         or NULL if not found
 */
static gchar*
xfce_keyboard_layout_get_option (gchar **options,
                                 gchar *option_name,
                                 gchar **_other_options)
{
    gchar **iter;
    gchar  *option_value  = NULL;
    gchar  *other_options = NULL;

    for (iter = options; iter && *iter; iter++)
    {
        if (g_str_has_prefix(*iter, option_name))
        {
            option_value = *iter;
        }
        else if (_other_options)
        {
            gchar *tmp = other_options;
            if (other_options)
            {
                other_options = g_strconcat(other_options, ",", *iter, NULL);
            }
            else
            {
                other_options = g_strdup(*iter);
            }
            g_free(tmp);
        }
    }

    *_other_options = other_options;
    return option_value;
}

static void
xfce_keyboard_layout_helper_set_grpkey (XfceKeyboardLayoutHelper *helper)
{
#ifdef HAVE_LIBXKLAVIER
    if (!helper->xkb_disable_settings)
    {
        gchar *grpkey;
        gchar *xkl_grpkey;
        gchar *other_options;

        xkl_grpkey = xfce_keyboard_layout_get_option (helper->config->options,
                                                      "grp:", &other_options);

        grpkey = xfconf_channel_get_string (helper->channel, "/Default/XkbOptions/Group",
                                            xkl_grpkey);
        if (g_strcmp0 (grpkey, xkl_grpkey) != 0)
        {
            gchar *options_string;
            if (other_options == NULL)
            {
                options_string = g_strdup (grpkey);
            }
            else
            {
                if (strlen(grpkey) != 0)
                {
                    options_string = g_strconcat (grpkey, ",", other_options, NULL);
                }
                else
                {
                    options_string = strdup(other_options);
                }
            }

            g_strfreev (helper->config->options);
            helper->config->options = g_strsplit(options_string, ",", 0);
            xkl_config_rec_activate (helper->config, helper->engine);

            xfsettings_dbg (XFSD_DEBUG_KEYBOARD_LAYOUT, "set grpkey to \"%s\"", grpkey);
            g_free(options_string);
        }

        g_free (other_options);
        g_free (grpkey);
    }
#endif /* HAVE_LIBXKLAVIER */
}

static void
xfce_keyboard_layout_helper_channel_property_changed (XfconfChannel      *channel,
                                               const gchar               *property_name,
                                               const GValue              *value,
                                               XfceKeyboardLayoutHelper  *helper)
{
    g_return_if_fail (helper->channel == channel);

    if (strcmp (property_name, "/Default/XkbDisable") == 0)
    {
        helper->xkb_disable_settings = g_value_get_boolean (value);
        /* Apply all settings */
        xfce_keyboard_layout_helper_set_model (helper);
        xfce_keyboard_layout_helper_set_layout (helper);
        xfce_keyboard_layout_helper_set_variant (helper);
        xfce_keyboard_layout_helper_set_grpkey (helper);
    }
    else if (strcmp (property_name, "/Default/XkbModel") == 0)
    {
        xfce_keyboard_layout_helper_set_layout (helper);
    }
    else if (strcmp (property_name, "/Default/XkbLayout") == 0)
    {
        xfce_keyboard_layout_helper_set_layout (helper);
    }
    else if (strcmp (property_name, "/Default/XkbVariant") == 0)
    {
        xfce_keyboard_layout_helper_set_variant (helper);
    }
    else if (strcmp (property_name, "/Default/XkbOptions/Group") == 0)
    {
        xfce_keyboard_layout_helper_set_grpkey (helper);
    }

    xfce_keyboard_layout_helper_process_xmodmap ();
}

static GdkFilterReturn
handle_xevent (GdkXEvent * xev, GdkEvent * event, XfceKeyboardLayoutHelper *helper)
{
#ifdef HAVE_LIBXKLAVIER
    XEvent *xevent = (XEvent *) xev;
    xkl_engine_filter_events (helper->engine, xevent);
#endif /* HAVE_LIBXKLAVIER */

    return GDK_FILTER_CONTINUE;
}

static void
xfce_keyboard_layout_reset_xkl_config (XklEngine *xklengine,
                                       XfceKeyboardLayoutHelper *helper)
{
    if (!helper->xkb_disable_settings)
    {
        xfsettings_dbg (XFSD_DEBUG_KEYBOARD_LAYOUT,
                        "New keyboard detected; restoring XKB settings.");

        xfce_keyboard_layout_helper_set_model (helper);
        xfce_keyboard_layout_helper_set_layout (helper);
        xfce_keyboard_layout_helper_set_variant (helper);
        xfce_keyboard_layout_helper_set_grpkey (helper);

        xfce_keyboard_layout_helper_process_xmodmap ();
    }
}
