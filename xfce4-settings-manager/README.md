# xfce4-settings-manager

The Settings Manager provides a unified view of all the different Xfce
settings dialogs.

## Registering Your Settings Dialog

In order to have your settings dialog included in the Settings Manager,
you need to add some extra things to the dialog's `.desktop` file.  You
can also optionally implement an embedding protocol.

### Categories

First, you need to add to the `Categories` key both the `Settings` and
`X-XFCE-SettingsDialog` values.  Additionally, you can add *one* of the
following categories:

* `X-XFCE-PersonalSettings`
* `X-XFCE-HardwareSettings`
* `X-XFCE-SystemSettings`

This last bit is optional, but will instruct the Settings Manager to
display the icon for your dialog in a particular category (Personal,
Hardware, or System).  If you don't include one of these, it will be
placed in a catch-all "Other" category.

### Documentation Link Keys

You can also add more information about your application via some
additional `.desktop` file keys:

* `X-XfceHelpComponent`
* `X-XfceHelpPage`
* `X-XfceHelpVersion`

These will set the behavior of the Help button in the Settings Manager
to load a page from `https://docs.xfce.org/`.  For most Xfce components,
the `Component` key should be the component name (e.g., `xfdesktop` or
`xfce4-panel`), the `Page` key should be `start`, and the `Version` key
should be the `$MAJOR.$MINOR` version of the component.

### Dialog Embedding

By default, clicking a settings dialog icon in the Settings Manager will
simply launch that settings application and display its contents in a
new window.  If you want, however, you can embed your dialog's contents
into the Settings Manager itself.  There are two ways to do this: an
out-of-process XEMBED protocol (which only works on X11) and an
in-process shared-library method (which should work everywhere).

To indicate that your dialog supports embedding, add
`X-XfcePluggable=true`, which declares your settings dialog supports the
X11-only out-of-process protocol.  You can additionally add
`X-XfcePluggableInProcess=true` to declare support for the in-process
method.

#### In-Process Method

If you want to support the in-process method, you should install a
shared library to `$(libdir)/xfce4/settings/dialogs/`.  The library
should be named the same as the `.desktop` file ID.  That is, if your
desktop file is named `xfce-backdrop-settings.desktop`, the shared
library should be named `libxfce-backdrop-settings.so`.

This shared library must export one symbol:

```c
GtkWidget *xfce_settings_dialog_impl_get_dialog_widget(GError **error);
```

This function should return a `GtkWidget` of the dialog's contents.
Often this will be a `GtkBox`, or perhaps a `GtkNotebook`.  Note that
this widget should not contain any dialog buttons (such as Help or
Close); the Settings Manager will handle displaying them.

If there is an error and you need to return `NULL` from this function,
you must also set `*error` to a valid `GError` instance describing the
problem.

As the Settings Manager will load and unload your shared library at
runtime, ensure that, when the returned `GtkWidget` is destroyed, all
other memory and resources relating to your dialog are also freed.  The
easiest way to do this is by using `g_object_weak_ref()` on your
`GtkWidget`; the Settings Manager will destroy tha widget right before
it unloads the shared library.

Additionally, in order to support having a standalone binary that the
user can run to start the settings dialog, you can create a shell script
similar to the following:

```bash
#!/bin/sh

exec xfce4-settings-manager --standalone --dialog $DIALOG_NAME
```

... where `$DIALOG_NAME` should be that `.desktop` file name (without
the `.deskop` extension).  You can then install that to `$(bindir)`, and
it can be what the `Exec` key in your `.desktop` file runs.

#### Out-of-Process Protocol

You can also use the older out-of-process protocol.  This will only work
on X11, and so should not be used for newer dialogs.

You should create a fully-fledged standalone dialog application as
usual, with a binary with a `main()` method that creates the dialog,
including Help and Close buttons, shows the dialog, and calls
`gtk_main()`.

From there, allow it to accept a `--socket-id` command-line parameter,
which accepts an integer argument.  When that parameter is passed,
instead of creating a full dialog, you should just create the dialog's
contents (excluding the toplevel window and any dialog buttons).  Pack
that widget into a `GtkPlug` that you initialize with the passed socket
ID.  You should connect to the plug's `delete-event` signal to know when
the Settings Manager has discarded your dialog's contents so you can
exit.

Additionally, you will want to call `gdk_notify_startup_complete()`, as
the Settings Manager will "fake" startup notification while the dialog
starts up.
