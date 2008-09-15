/*
 * Copyright © 2001 Red Hat, Inc.
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Red Hat not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  Red Hat makes no representations about the
 * suitability of this software for any purpose.  It is provided "as is"
 * without express or implied warranty.
 *
 * RED HAT DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL RED HAT
 * BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN 
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Author:  Owen Taylor, Red Hat, Inc.
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
 *          Stephan Arts <stephan@xfce.org>: adapted to the "xfconf" concept
 */

#include <config.h>
#include <X11/Xlib.h>
#include <X11/Xmd.h>

#include <glib.h>

#if defined(GETTEXT_PACKAGE)
#include <glib/gi18n-lib.h>
#else
#include <glib/gi18n.h>
#endif

#include <gtk/gtk.h>
#include <gdk/gdkx.h>

#include <string.h>

#include <libxfce4util/libxfce4util.h>
#include <xfconf/xfconf.h>

#include "registry.h"

#define XSETTINGS_PAD(n,m) ((n + m - 1) & (~(m-1)))

#define XSETTINGS_DEBUG(str) \
    if (debug) g_print ("%s", str)
#define XSETTINGS_DEBUG_CREATE(str) \
    if (debug) g_print ("Creating Property: '%s'\n", str)
#define XSETTINGS_DEBUG_LOAD(str) \
    if (debug) g_print ("Loading Property: '%s'\n", str)

#define XSETTINGS_PARAM_FLAGS (G_PARAM_READWRITE \
                               | G_PARAM_CONSTRUCT_ONLY \
                               | G_PARAM_STATIC_NAME \
                               | G_PARAM_STATIC_NICK \
                               | G_PARAM_STATIC_BLURB)

G_DEFINE_TYPE(XSettingsRegistry, xsettings_registry, G_TYPE_OBJECT);

enum
{
    XSETTING_ENTRY_NET_DOUBLECLICKTIME,
    XSETTING_ENTRY_NET_DOUBLECLICKDISTANCE,
    XSETTING_ENTRY_NET_DNDDRAGTRESHOLD,
    XSETTING_ENTRY_NET_CURSORBLINK,
    XSETTING_ENTRY_NET_CURSORBLINKTIME,
    XSETTING_ENTRY_NET_THEMENAME,
    XSETTING_ENTRY_NET_ICONTHEMENAME,
    XSETTING_ENTRY_XFT_ANTIALIAS,
    XSETTING_ENTRY_XFT_HINTING,
    XSETTING_ENTRY_XFT_HINTSTYLE,
    XSETTING_ENTRY_XFT_RGBA,
    XSETTING_ENTRY_XFT_DPI,
    XSETTING_ENTRY_GTK_CANCHANGEACCELS,
    XSETTING_ENTRY_GTK_COLORPALETTE,
    XSETTING_ENTRY_GTK_FONTNAME,
    XSETTING_ENTRY_GTK_ICONSIZES,
    XSETTING_ENTRY_GTK_KEYTHEMENAME,
    XSETTING_ENTRY_GTK_TOOLBARSTYLE,
    XSETTING_ENTRY_GTK_TOOLBARICONSIZE,
    XSETTING_ENTRY_GTK_IMPREEDITSTYLE,
    XSETTING_ENTRY_GTK_IMSTATUSSTYLE,
    XSETTING_ENTRY_GTK_MENUIMAGES,
    XSETTING_ENTRY_GTK_BUTTONIMAGES,
    XSETTING_ENTRY_GTK_MENUBARACCEL,
    XSETTING_ENTRY_GTK_CURSORTHEMENAME,
    XSETTING_ENTRY_GTK_CURSORTHEMESIZE,
} XSettingType;

static XSettingsRegistryEntry properties[] = {
{ "Net/DoubleClickTime", {G_TYPE_INT, }},
{ "Net/DoubleClickDistance", {G_TYPE_INT, }},
{ "Net/DndDragThreshold", {G_TYPE_INT, }},
{ "Net/CursorBlink", {G_TYPE_BOOLEAN, }},
{ "Net/CursorBlinkTime", {G_TYPE_INT, }},
{ "Net/ThemeName", {G_TYPE_STRING, }},
{ "Net/IconThemeName", {G_TYPE_STRING, }},

{ "Xft/Antialias", {G_TYPE_INT, }},
{ "Xft/Hinting", {G_TYPE_INT, }},
{ "Xft/HintStyle", {G_TYPE_STRING, }},
{ "Xft/RGBA", {G_TYPE_STRING, }},
{ "Xft/DPI", {G_TYPE_INT, }},

{ "Gtk/CanChangeAccels", {G_TYPE_BOOLEAN, }},
{ "Gtk/ColorPalette", {G_TYPE_STRING, }},
{ "Gtk/FontName", {G_TYPE_STRING, }},
{ "Gtk/IconSizes", {G_TYPE_STRING, }},
{ "Gtk/KeyThemeName", {G_TYPE_STRING, }},
{ "Gtk/ToolbarStyle", {G_TYPE_STRING, }},
{ "Gtk/ToolbarIconSize", {G_TYPE_INT, }},
{ "Gtk/IMPreeditStyle", {G_TYPE_STRING, }},
{ "Gtk/IMStatusStyle", {G_TYPE_STRING, }},
{ "Gtk/MenuImages", {G_TYPE_BOOLEAN, }},
{ "Gtk/ButtonImages", {G_TYPE_BOOLEAN, }},
{ "Gtk/MenuBarAccel", {G_TYPE_STRING, }},
{ "Gtk/CursorThemeName", {G_TYPE_STRING, }},
{ "Gtk/CursorThemeSize", {G_TYPE_INT, }},

{ NULL, {0, }},
};


struct _XSettingsRegistryPriv
{
    gint serial;
    gint last_change_serial;

    /* props */
    XfconfChannel *channel;
    gint screen;
    Display *display;
    Window window;
    Atom xsettings_atom;
    Atom selection_atom;
};

static void xsettings_registry_set_property(GObject*, guint, const GValue*, GParamSpec*);
static void xsettings_registry_get_property(GObject*, guint, GValue*, GParamSpec*);

static void
cb_xsettings_registry_channel_property_changed(XfconfChannel *channel, const gchar *property_name, const GValue *value, XSettingsRegistry *registry); 
static Bool
timestamp_predicate (Display *display, XEvent  *xevent, XPointer arg);

gboolean
xsettings_registry_process_event (XSettingsRegistry *registry, XEvent *xevent)
{
    if ((xevent->xany.window == registry->priv->window) &&
        (xevent->xany.type == SelectionClear) &&
        (xevent->xselectionclear.selection == registry->priv->selection_atom))
    {
        return TRUE;
    }

    return FALSE;
}

enum {
	XSETTINGS_REGISTRY_PROPERTY_CHANNEL = 1,
    XSETTINGS_REGISTRY_PROPERTY_DISPLAY,
    XSETTINGS_REGISTRY_PROPERTY_SCREEN,
    XSETTINGS_REGISTRY_PROPERTY_XSETTINGS_ATOM,
    XSETTINGS_REGISTRY_PROPERTY_SELECTION_ATOM,
    XSETTINGS_REGISTRY_PROPERTY_WINDOW
};

void
xsettings_registry_class_init(XSettingsRegistryClass *reg_class)
{
	GObjectClass *object_class = G_OBJECT_CLASS(reg_class);
	GParamSpec *pspec;

	object_class->set_property = xsettings_registry_set_property;
	object_class->get_property = xsettings_registry_get_property;

	pspec = g_param_spec_object("channel", NULL, NULL, XFCONF_TYPE_CHANNEL, XSETTINGS_PARAM_FLAGS);
	g_object_class_install_property(object_class, XSETTINGS_REGISTRY_PROPERTY_CHANNEL, pspec);

	pspec = g_param_spec_int("screen", NULL, NULL, -1, 65535, -1, XSETTINGS_PARAM_FLAGS);
	g_object_class_install_property(object_class, XSETTINGS_REGISTRY_PROPERTY_SCREEN, pspec);

	pspec = g_param_spec_pointer("display", NULL, NULL, XSETTINGS_PARAM_FLAGS);
	g_object_class_install_property(object_class, XSETTINGS_REGISTRY_PROPERTY_DISPLAY, pspec);

	pspec = g_param_spec_long("xsettings-atom", NULL, NULL, G_MINLONG, G_MAXLONG, 0, XSETTINGS_PARAM_FLAGS);
	g_object_class_install_property(object_class, XSETTINGS_REGISTRY_PROPERTY_XSETTINGS_ATOM, pspec);

	pspec = g_param_spec_long("selection-atom", NULL, NULL, G_MINLONG, G_MAXLONG, 0, XSETTINGS_PARAM_FLAGS);
	g_object_class_install_property(object_class, XSETTINGS_REGISTRY_PROPERTY_SELECTION_ATOM, pspec);

	pspec = g_param_spec_long("window", NULL, NULL, G_MINLONG, G_MAXLONG, 0, XSETTINGS_PARAM_FLAGS);
	g_object_class_install_property(object_class, XSETTINGS_REGISTRY_PROPERTY_WINDOW, pspec);
}

void
xsettings_registry_init(XSettingsRegistry *registry)
{
    registry->priv = g_new0(XSettingsRegistryPriv, 1);

    /* Net settings */
    g_value_set_int (&properties[XSETTING_ENTRY_NET_DOUBLECLICKTIME].value, 250);
    g_value_set_int (&properties[XSETTING_ENTRY_NET_DOUBLECLICKDISTANCE].value, 5);
    g_value_set_int (&properties[XSETTING_ENTRY_NET_DNDDRAGTRESHOLD].value, 8);
    g_value_set_boolean (&properties[XSETTING_ENTRY_NET_CURSORBLINK].value, TRUE);
    g_value_set_int (&properties[XSETTING_ENTRY_NET_CURSORBLINKTIME].value, 1200);
    g_value_set_string (&properties[XSETTING_ENTRY_NET_THEMENAME].value, "Default");
    g_value_set_string (&properties[XSETTING_ENTRY_NET_ICONTHEMENAME].value, "hicolor");
    /* Xft settings */
    g_value_set_int (&properties[XSETTING_ENTRY_XFT_ANTIALIAS].value, -1);
    g_value_set_int (&properties[XSETTING_ENTRY_XFT_HINTING].value, -1);
    g_value_set_string (&properties[XSETTING_ENTRY_XFT_HINTSTYLE].value, "hintnone");
    g_value_set_string (&properties[XSETTING_ENTRY_XFT_RGBA].value, "none");
    g_value_set_int (&properties[XSETTING_ENTRY_XFT_DPI].value, -1);
    /* GTK settings */
    g_value_set_boolean (&properties[XSETTING_ENTRY_GTK_CANCHANGEACCELS].value, FALSE);
    g_value_set_string (&properties[XSETTING_ENTRY_GTK_COLORPALETTE].value,
                    "black:white:gray50:red:purple:blue:light "
                    "blue:green:yellow:orange:lavender:brown:goldenrod4:dodger "
                    "blue:pink:light green:gray10:gray30:gray75:gray90");
    g_value_set_string (&properties[XSETTING_ENTRY_GTK_FONTNAME].value, "Sans 10");
    g_value_set_string (&properties[XSETTING_ENTRY_GTK_ICONSIZES].value, NULL);
    g_value_set_string (&properties[XSETTING_ENTRY_GTK_KEYTHEMENAME].value, NULL);
    g_value_set_string (&properties[XSETTING_ENTRY_GTK_TOOLBARSTYLE].value, "Icons");
    g_value_set_int (&properties[XSETTING_ENTRY_GTK_TOOLBARICONSIZE].value, 3);
    g_value_set_string (&properties[XSETTING_ENTRY_GTK_IMPREEDITSTYLE].value, "");
    g_value_set_string (&properties[XSETTING_ENTRY_GTK_IMSTATUSSTYLE].value, "");
    g_value_set_boolean (&properties[XSETTING_ENTRY_GTK_MENUIMAGES].value, TRUE);
    g_value_set_boolean (&properties[XSETTING_ENTRY_GTK_BUTTONIMAGES].value, TRUE);
    g_value_set_string (&properties[XSETTING_ENTRY_GTK_MENUBARACCEL].value, "F10");

    g_value_set_string (&properties[XSETTING_ENTRY_GTK_CURSORTHEMENAME].value, NULL);
    g_value_set_int (&properties[XSETTING_ENTRY_GTK_CURSORTHEMESIZE].value, 0);
}

static void
cb_xsettings_registry_channel_property_changed(XfconfChannel *channel, const gchar *name, const GValue *value, XSettingsRegistry *registry)
{
    XSettingsRegistryEntry *entry = properties;

    for(; entry->name != NULL; ++entry)
    {
        if (!strcmp(entry->name, &name[1]))
        {
            g_value_reset(&entry->value);
            g_value_copy(value, &entry->value);
            break;
        }
    }
    xsettings_registry_notify(registry);
    if (!strncmp(name, "/Xft", 4) || !strncmp(name, "/Gtk/CursorTheme", 16))
        xsettings_registry_store_xrdb(registry);
}

void
xsettings_registry_store_xrdb(XSettingsRegistry *registry)
{
    gchar    *filename;
    GError   *error = NULL;
    GString  *string;
    gchar    *command, *contents;
    gboolean  result = TRUE;

    /* store the xft properties */
    filename = xfce_resource_save_location(XFCE_RESOURCE_CONFIG, "xfce4" G_DIR_SEPARATOR_S "Xft.xrdb", TRUE);
    if (G_LIKELY (filename))
    {
        /* create file contents */
        string = g_string_sized_new (80);
        g_string_append_printf (string, "Xft.antialias: %d\n"
                                        "Xft.hinting: %d\n"
                                        "Xft.rgba: %s\n",
                                        g_value_get_int (&properties[XSETTING_ENTRY_XFT_ANTIALIAS].value),
                                        g_value_get_int (&properties[XSETTING_ENTRY_XFT_HINTING].value),
                                        g_value_get_string (&properties[XSETTING_ENTRY_XFT_RGBA].value));
        
        if (g_value_get_int (&properties[XSETTING_ENTRY_XFT_HINTING].value))
            g_string_append_printf (string, "Xft.hintstyle: %s\n", g_value_get_string (&properties[XSETTING_ENTRY_XFT_HINTSTYLE].value));
        else
            string = g_string_append (string, "Xft.hintstyle: hintnone\n");
            
        if (g_value_get_int (&properties[XSETTING_ENTRY_XFT_DPI].value) > 0)
            g_string_append_printf (string, "Xft.dpi: %d\n", g_value_get_int (&properties[XSETTING_ENTRY_XFT_DPI].value));

        /* try to write the file contents */
        if (G_LIKELY (g_file_set_contents (filename, string->str, -1, &error)))
        {
            /* create command to merge with the x resource database */
            command = g_strdup_printf ("xrdb -nocpp -merge \"%s\"", filename);
            result = g_spawn_command_line_async (command, &error);
            g_free (command);
            
            /* remove dpi from the database if not set */
            if (result && g_value_get_int (&properties[XSETTING_ENTRY_XFT_DPI].value) == 0)
            {
                command = g_strdup ("sh -c \"xrdb -query | grep -i -v '^Xft.dpi:' | xrdb\"");
                result = g_spawn_command_line_async (command, &error);
                g_free (command);
            }
        }
        else
        {
            /* print error */
            g_critical ("Failed to write to '%s': %s", filename, error->message);
            g_error_free (error);
        }
        
        /* cleanup */
        g_free (filename);
        g_string_free (string, TRUE);
        
        /* leave when there where spawn problems */
        if (result == FALSE)
            goto spawn_error;
    }
    
    /* store cursor settings */
    filename = xfce_resource_save_location(XFCE_RESOURCE_CONFIG, "xfce4" G_DIR_SEPARATOR_S "Xcursor.xrdb", TRUE);
    if (G_LIKELY (filename))
    {
        /* build file contents */
        contents = g_strdup_printf ("Xcursor.theme: %s\n"
                                    "Xcursor.theme_core: true\n"
                                    "Xcursor.size: %d\n",
                                    g_value_get_string (&properties[XSETTING_ENTRY_GTK_CURSORTHEMENAME].value),
                                    g_value_get_int (&properties[XSETTING_ENTRY_GTK_CURSORTHEMESIZE].value));
        
        /* write the contents to the file */
        if (G_LIKELY (g_file_set_contents (filename, contents, -1, &error)))
        {
            /* create command to merge with the x resource database */
            command = g_strdup_printf ("xrdb -nocpp -merge \"%s\"", filename);
            result = g_spawn_command_line_async (command, &error);
            g_free (command);
        }
        else
        {
            /* print error */
            g_critical ("Failed to write to '%s': %s", filename, error->message);
            g_error_free (error);
        }
        
        /* cleanup */
        g_free (filename);
        g_free (contents);
        
        /* leave when there where spawn problems */
        if (result == FALSE)
            goto spawn_error;
    }
    
    /* leave */
    return;
    
    spawn_error:
    
    /* print warning */
    g_critical ("Failed to spawn xrdb: %s", error->message);
    g_error_free (error);
}

void
xsettings_registry_notify(XSettingsRegistry *registry)
{
    guchar *buffer, *pos;
    gint buf_len;

    gint prop_count = 0;
    registry->priv->last_change_serial = registry->priv->serial;

    XSettingsRegistryEntry *entry = properties;

    buf_len = 12;

    /** Calculate buffer size */
    for(; entry->name != NULL; ++entry)
    {
        prop_count++;
        buf_len += 8 + XSETTINGS_PAD(strlen(entry->name), 4);
        switch (G_VALUE_TYPE(&entry->value))
        {
            case G_TYPE_INT:
            case G_TYPE_BOOLEAN:
                buf_len += 4;
                break;
            case G_TYPE_STRING:
                {
                    buf_len += 4;
                    const gchar *value = g_value_get_string(&entry->value);
                    if(value)
                    {
                        buf_len += XSETTINGS_PAD(strlen(value), 4);
                    }

                }
                break;
            case G_TYPE_UINT64:
                buf_len += 8;
                break;
        }
    }

    buffer = NULL;
    pos = buffer = g_new0(guchar, buf_len);

#if G_BYTE_ORDER == G_LITTLE_ENDIAN
    *(CARD8 *)pos = LSBFirst;
#else
    *(CARD8 *)pos = MSBFirst;
#endif
    pos +=4;

    *(CARD32 *)pos = registry->priv->serial++;
    pos += 4;

    *(CARD32 *)pos = prop_count; /* nr of props */
    pos += 4;

    /** Fill the buffer */
    entry = properties;
    for(; entry->name != NULL; ++entry)
    {
        gint name_len, value_len = 0, str_length;


        name_len = XSETTINGS_PAD(strlen(entry->name), 4);
        value_len = 0;

        switch (G_VALUE_TYPE(&entry->value))
        {
            case G_TYPE_INT:
            case G_TYPE_BOOLEAN:
                *pos++ = 0;
                break;
            case G_TYPE_STRING:
                *pos++ = 1; // String 
                {
                    const gchar *value = g_value_get_string(&entry->value);
                    if(value)
                    {
                        value_len = XSETTINGS_PAD(strlen(value), 4);
                    }
                    else
                    {
                        value_len = 0;
                    }
                }
                break;
            case G_TYPE_UINT64: /* Color is a 64-bits value */
                *pos++ = 2;
                break;
        }
        *pos++ = 0;

        str_length = strlen(entry->name);
        *(CARD16 *)pos = str_length;
        pos += 2;
        memcpy (pos, entry->name, str_length);
        name_len -= str_length;
        pos += str_length;

        while(name_len > 0)
        {
            *(pos++) = 0;
            name_len--;
        }
        
        *(CARD32 *)pos = registry->priv->last_change_serial; 
        pos+= 4;

        switch (G_VALUE_TYPE(&entry->value))
        {
            case G_TYPE_STRING:
                {
                    const gchar *val = g_value_get_string(&entry->value);

                    if (val)
                    {
                        *(CARD32 *)pos = strlen(val);
                        pos += 4;

                        memcpy (pos, val, strlen(val));
                        pos += strlen(val);
                        value_len -= strlen(val);
                    }
                    else
                    {
                        *(CARD32 *)pos = 0;
                        pos += 4;
                    }
                }
                while(value_len > 0)
                {
                    *(pos++) = 0;
                    value_len--;
                }
                break;
            case G_TYPE_INT:
                /* See http://www.freedesktop.org/wiki/Specifications/XSettingsRegistry
                 * for an explanation.  Weirdly, this is still not correct, as
                 * font sizes with -1 DPI and a forced setting equal to the X
                 * server's calculated DPI don't match.  Need to look into
                 * this a bit more. */
                if (strcmp (entry->name, "Xft/DPI") == 0 && g_value_get_int(&entry->value) != -1)
                    *(CARD32 *)pos = g_value_get_int(&entry->value) * 1024;
                else
                    *(CARD32 *)pos = g_value_get_int(&entry->value);
                pos += 4;
                break;
            case G_TYPE_BOOLEAN:
                *(CARD32 *)pos = g_value_get_boolean(&entry->value);
                pos += 4;
                break;
            case G_TYPE_UINT64:

                pos += 8;
                break;
        }

    }

    XChangeProperty(registry->priv->display,
                    registry->priv->window,
                    registry->priv->xsettings_atom,
                    registry->priv->xsettings_atom,
                    8, PropModeReplace, buffer, buf_len);

    registry->priv->last_change_serial = registry->priv->serial;

    g_free (buffer);
}

XSettingsRegistry *
xsettings_registry_new (XfconfChannel *channel, Display *dpy, gint screen)
{
    Atom xsettings_atom = XInternAtom(dpy, "_XSETTINGS_SETTINGS", True);

    Window window = 0;
    gchar buffer[256];
    unsigned char c = 'a';
    TimeStampInfo info;
    XEvent xevent;
    Atom selection_atom, manager_atom;
    GObject *object;

    window = XCreateSimpleWindow (dpy,
					 RootWindow (dpy, screen),
					 0, 0, 10, 10, 0,
					 WhitePixel (dpy, screen),
					 WhitePixel (dpy, screen));
    if (!window)
    {
        g_critical( "no window");
        return NULL;
    }

    g_snprintf(buffer, sizeof(buffer), "_XSETTINGS_S%d", screen);
    selection_atom = XInternAtom(dpy, buffer, True);
    manager_atom = XInternAtom(dpy, "MANAGER", True);


    object = g_object_new(XSETTINGS_REGISTRY_TYPE,
                          "channel", channel,
                          "display", dpy,
                          "screen", screen,
                          "xsettings-atom", xsettings_atom,
                          "selection-atom", selection_atom,
                          "window", window,
                          NULL);

    info.timestamp_prop_atom = XInternAtom(dpy, "_TIMESTAMP_PROP", False);
    info.window = window;

    XSelectInput (dpy, window, PropertyChangeMask);

    XChangeProperty (dpy, window,
       info.timestamp_prop_atom, info.timestamp_prop_atom,
       8, PropModeReplace, &c, 1);

    XIfEvent (dpy, &xevent,
              timestamp_predicate, (XPointer)&info); 

    XSetSelectionOwner (dpy, selection_atom,
                        window, xevent.xproperty.time);

    if (XGetSelectionOwner (dpy, selection_atom) ==
        window)
    {
        XClientMessageEvent xev;

        xev.type = ClientMessage;
        xev.window = RootWindow (dpy, screen);
        xev.message_type = manager_atom;
        xev.format = 32;
        xev.data.l[0] = xevent.xproperty.time;
        xev.data.l[1] = selection_atom;
        xev.data.l[2] = window;
        xev.data.l[3] = 0;	/* manager specific data */
        xev.data.l[4] = 0;	/* manager specific data */

        XSendEvent (dpy, RootWindow (dpy, screen),
          False, StructureNotifyMask, (XEvent *)&xev);
    }
    else
    {
        g_debug("fail");
    }

    return XSETTINGS_REGISTRY(object);
}

static void
xsettings_registry_set_property(GObject *object, guint property_id, const GValue *value, GParamSpec *p_spec)
{
	switch(property_id)
	{
		case XSETTINGS_REGISTRY_PROPERTY_CHANNEL:
            if (XSETTINGS_REGISTRY(object)->priv->channel)
            {
                XfconfChannel *channel = XSETTINGS_REGISTRY(object)->priv->channel;

                g_signal_handlers_disconnect_by_func(G_OBJECT(channel), (GCallback)cb_xsettings_registry_channel_property_changed, object);
                XSETTINGS_REGISTRY(object)->priv->channel = NULL;
            }

			XSETTINGS_REGISTRY(object)->priv->channel = g_value_get_object(value);

            if (XSETTINGS_REGISTRY(object)->priv->channel)
            {
                XfconfChannel *channel = XSETTINGS_REGISTRY(object)->priv->channel;

                g_signal_connect(G_OBJECT(channel), "property-changed", (GCallback)cb_xsettings_registry_channel_property_changed, object);
            }
		    break;
		case XSETTINGS_REGISTRY_PROPERTY_SCREEN:
            XSETTINGS_REGISTRY(object)->priv->screen = g_value_get_int(value);
            break;
		case XSETTINGS_REGISTRY_PROPERTY_DISPLAY:
            XSETTINGS_REGISTRY(object)->priv->display = g_value_get_pointer(value);
            break;
        case XSETTINGS_REGISTRY_PROPERTY_XSETTINGS_ATOM:
            XSETTINGS_REGISTRY(object)->priv->xsettings_atom = g_value_get_long(value);
            break;
        case XSETTINGS_REGISTRY_PROPERTY_SELECTION_ATOM:
            XSETTINGS_REGISTRY(object)->priv->selection_atom = g_value_get_long(value);
            break;
        case XSETTINGS_REGISTRY_PROPERTY_WINDOW:
            XSETTINGS_REGISTRY(object)->priv->window = g_value_get_long(value);
            break;
	}

}

static void
xsettings_registry_get_property(GObject *object, guint property_id, GValue *value, GParamSpec *p_spec)
{
	switch(property_id)
	{
		case XSETTINGS_REGISTRY_PROPERTY_CHANNEL:
			g_value_set_object(value, XSETTINGS_REGISTRY(object)->priv->channel);
		break;
		case XSETTINGS_REGISTRY_PROPERTY_SCREEN:
            g_value_set_int(value, XSETTINGS_REGISTRY(object)->priv->screen);
        break;
		case XSETTINGS_REGISTRY_PROPERTY_DISPLAY:
            g_value_set_pointer(value, XSETTINGS_REGISTRY(object)->priv->display);
        break;
        case XSETTINGS_REGISTRY_PROPERTY_XSETTINGS_ATOM:
            g_value_set_long(value, XSETTINGS_REGISTRY(object)->priv->xsettings_atom);
        break;
        case XSETTINGS_REGISTRY_PROPERTY_SELECTION_ATOM:
            g_value_set_long(value, XSETTINGS_REGISTRY(object)->priv->selection_atom);
        break;
        case XSETTINGS_REGISTRY_PROPERTY_WINDOW:
            g_value_set_long(value, XSETTINGS_REGISTRY(object)->priv->window);
        break;
	}

}

static Bool
timestamp_predicate (Display *display,
		     XEvent  *xevent,
		     XPointer arg)
{
  TimeStampInfo *info = (TimeStampInfo *)arg;

  if (xevent->type == PropertyNotify &&
      xevent->xproperty.window == info->window &&
      xevent->xproperty.atom == info->timestamp_prop_atom)
    return True;

  return False;
}

gboolean
xsettings_registry_load(XSettingsRegistry *registry, gboolean debug)
{
    XfconfChannel *channel = registry->priv->channel;
    XSettingsRegistryEntry *entry = properties;
    gchar *str;

    while (entry->name)
    {
        gchar *name = g_strconcat("/", entry->name, NULL);

        if (xfconf_channel_has_property(channel, name) == TRUE)
        {
            XSETTINGS_DEBUG_LOAD(entry->name);
            switch (G_VALUE_TYPE(&entry->value))
            {
                case G_TYPE_INT:
                    g_value_set_int(&entry->value, xfconf_channel_get_int(channel, name, g_value_get_int(&entry->value)));
                    break;
                case G_TYPE_STRING:
                    str = xfconf_channel_get_string(channel, name, g_value_get_string(&entry->value));
                    g_value_set_string(&entry->value, str);
                    g_free(str);
                    break;
                case G_TYPE_BOOLEAN:
                    g_value_set_boolean(&entry->value, xfconf_channel_get_bool(channel, name, g_value_get_boolean(&entry->value)));
                    break;
            }
        }
        else
        {
            XSETTINGS_DEBUG_CREATE(entry->name);

            if(xfconf_channel_set_property(channel, name, &entry->value))
            {
                XSETTINGS_DEBUG("... OK\n");
            }
            else
            {
                XSETTINGS_DEBUG("... FAIL\n");
            }
        }

        g_free(name);
        entry++;
    }

    return TRUE;
}
