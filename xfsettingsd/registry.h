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
 */

#ifndef __XSETTINGS_REGISTRY_H__
#define __XSETTINGS_REGISTRY_H__

typedef struct _XSettingsRegistryEntry XSettingsRegistryEntry;

struct _XSettingsRegistryEntry {
    gchar *name;
    GValue value;
};

#define XSETTINGS_REGISTRY_TYPE xsettings_registry_get_type()

#define XSETTINGS_REGISTRY(obj) (			   \
		G_TYPE_CHECK_INSTANCE_CAST ((obj),  \
			XSETTINGS_REGISTRY_TYPE,				  \
			XSettingsRegistry))

#define XSETTINGS_IS_REGISTRY(obj) (			\
		G_TYPE_CHECK_INSTANCE_TYPE ((obj),  \
			XSETTINGS_REGISTRY_TYPE))

#define XSETTINGS_REGISTRY_CLASS(class) (	  \
		G_TYPE_CHECK_CLASS_CAST ((class),  \
			XSETTINGS_REGISTRY_TYPE,				 \
			XSettingsRegistryClass))

#define XSETTINGS_IS_REGISTRY_CLASS(class) (   \
		G_TYPE_CHECK_CLASS_TYPE ((class),  \
			XSETTINGS_REGISTRY_TYPE))

#define XSETTINGS_REGISTRY_GET_CLASS(obj) (	\
		G_TYPE_INSTANCE_GET_CLASS ((obj),  \
			XSETTINGS_REGISTRY_TYPE,				 \
	  XSettingsRegistryClass))

typedef struct 
{
  Window window;
  Atom timestamp_prop_atom;
} TimeStampInfo;


typedef struct _XSettingsRegistryPriv XSettingsRegistryPriv;
typedef struct _XSettingsRegistry XSettingsRegistry;

struct _XSettingsRegistry {
    GObject parent;
    XSettingsRegistryPriv *priv;
};

typedef struct _XSettingsRegistryClass XSettingsRegistryClass;

struct _XSettingsRegistryClass {
    GObjectClass parent_class;
};

GType xsettings_registry_get_type(void);

XSettingsRegistry *
xsettings_registry_new (XfconfChannel *channel, Display *dpy, gint screen);

gboolean
xsettings_registry_load(XSettingsRegistry *registry, gboolean debug);
void
xsettings_registry_notify(XSettingsRegistry *registry);
void
xsettings_registry_store_xrdb(XSettingsRegistry *registry);
gboolean
xsettings_registry_process_event (XSettingsRegistry *registry, XEvent *xevent);

#endif /* __XSETTINGS_REGISTRY_H__ */
