/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * Copyright (C) 2022 Fyra Labs
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "config.h"

#include <string.h>
#include <glib/gi18n-lib.h>
#include <glib.h>
#include <gio/gio.h>
#include <gio/gdesktopappinfo.h>

#include "cc-extensions-panel.h"
#include "cc-extensions-resources.h"
#include "shell/cc-application.h"
#include "shell/cc-object-storage.h"

struct _CcExtensionsPanel {
  CcPanel                 parent_instance;

  GtkSwitch               *enabled_switch;

  GSettings               *dock_settings;
  GDBusProxy              *extensions_proxy;
};

CC_PANEL_REGISTER (CcExtensionsPanel, cc_extensions_panel);

static void
cc_dock_panel_dispose (GObject *object)
{
  CcExtensionsPanel *self = CC_EXTENSIONS_PANEL (object);

  g_clear_object (&self->dock_settings);
  g_clear_object (&self->extensions_proxy);

  G_OBJECT_CLASS (cc_extensions_panel_parent_class)->dispose (object);
}

static void
got_extensions_proxy_cb (GObject      *source_object,
                        GAsyncResult *res,
                        gpointer      data)
{
  g_autoptr(GError) error = NULL;
  CcExtensionsPanel *self = data;

  self->extensions_proxy = g_dbus_proxy_new_for_bus_finish (res, &error);

  if (self->extensions_proxy == NULL)
    {
      g_warning ("Error creating proxy: %s", error->message);
      return;
    }
}

static void
on_view_dock_settings_clicked_cb (CcExtensionsPanel *self)
{
  g_autoptr (GError) error = NULL;

  if (!self->extensions_proxy)
    return;

  g_dbus_proxy_call_sync (self->extensions_proxy,
                          "OpenExtensionsPrefs",
                          g_variant_new("(ssa{sv})", "dash-to-dock@tauos.co", "", NULL),
                          0,
                          -1,
                          NULL,
                          &error);

  if (error)
    g_warning ("Error opening extensions settings: %s", error->message);
}

static void
cc_extensions_panel_class_init (CcExtensionsPanelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = cc_dock_panel_dispose;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/extensions/cc-extensions-panel.ui");

  gtk_widget_class_bind_template_child (widget_class, CcExtensionsPanel, enabled_switch);
}

static void
cc_extensions_panel_init (CcExtensionsPanel *self)
{
  g_autoptr(GSettingsSchema) schema = NULL;

  g_resources_register (cc_extensions_get_resource ());

  gtk_widget_init_template (GTK_WIDGET (self));
  /* Only load if we have dash to dock installed */
  schema = g_settings_schema_source_lookup (g_settings_schema_source_get_default (), "org.gnome.shell.extensionss.dash-to-dock", TRUE);
  if (!schema)
    {
      g_warning ("No dock is installed here. Panel disabled. Please fix your installation.");
      return;
    }

  self->dock_settings = g_settings_new_full (schema, NULL, NULL);

  // g_settings_bind (self->dock_settings, "dock-fixed",
  //                  self->dock_autohide_switch, "active",
  //                  G_SETTINGS_BIND_INVERT_BOOLEAN);

  g_dbus_proxy_new_for_bus (G_BUS_TYPE_SESSION,
                            G_DBUS_PROXY_FLAGS_NONE,
                            NULL,
                            "org.gnome.Shell.Extensionss",
                            "/org/gnome/Shell/Extensionss",
                            "org.gnome.Shell.Extensionss",
                            NULL,
                            got_extensions_proxy_cb,
                            self);

  const prop = g_dbus_proxy_get_cached_property (self->extensions_proxy,
                                    "UserExtensionsEnabled");

  g_warning ("sf %s", prop);
}