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

#include "cc-dock-panel.h"
#include "cc-dock-resources.h"
#include "shell/cc-application.h"
#include "shell/cc-object-storage.h"

#define ICONSIZE_KEY "dash-max-icon-size"

struct _CcDockPanel {
  CcPanel                 parent_instance;

  GtkSwitch               *dock_autohide_switch;
  GtkRadioButton           *icon_size_32;
  GtkRadioButton           *icon_size_48;
  GtkRadioButton           *icon_size_64;
  GtkScale                *icon_size_scale;
  AdwComboRow             *dock_position_combo;

  GSettings               *dock_settings;
  GDBusProxy              *extension_proxy;
};

CC_PANEL_REGISTER (CcDockPanel, cc_dock_panel);

static void
cc_dock_panel_dispose (GObject *object)
{
  CcDockPanel *self = CC_DOCK_PANEL (object);

  g_clear_object (&self->dock_settings);
  g_clear_object (&self->extension_proxy);

  G_OBJECT_CLASS (cc_dock_panel_parent_class)->dispose (object);
}

static void
got_extension_proxy_cb (GObject      *source_object,
                        GAsyncResult *res,
                        gpointer      data)
{
  g_autoptr(GError) error = NULL;
  CcDockPanel *self = data;

  self->extension_proxy = g_dbus_proxy_new_for_bus_finish (res, &error);

  if (self->extension_proxy == NULL)
    {
      g_warning ("Error creating proxy: %s", error->message);
      return;
    }
}

static void
on_view_dock_settings_clicked_cb (CcDockPanel *self)
{
  g_autoptr (GError) error = NULL;

  if (!self->extension_proxy)
    return;

  g_dbus_proxy_call_sync (self->extension_proxy,
                          "OpenExtensionPrefs",
                          g_variant_new("(ssa{sv})", "dash-to-dock@tauos.co", "", NULL),
                          0,
                          -1,
                          NULL,
                          &error);

  if (error)
    g_warning ("Error opening extension settings: %s", error->message);
}

static void
on_icon_size_32_changed (CcDockPanel *self)
{
  gint value = 32;
  if (g_settings_get_int (self->dock_settings, ICONSIZE_KEY) != value)
    g_settings_set_int (self->dock_settings, ICONSIZE_KEY, value);
}

static void
on_icon_size_48_changed (CcDockPanel *self)
{
  gint value = 48;
  if (g_settings_get_int (self->dock_settings, ICONSIZE_KEY) != value)
    g_settings_set_int (self->dock_settings, ICONSIZE_KEY, value);
}

static void
on_icon_size_64_changed (CcDockPanel *self)
{
  gint value = 64;
  if (g_settings_get_int (self->dock_settings, ICONSIZE_KEY) != value)
    g_settings_set_int (self->dock_settings, ICONSIZE_KEY, value);
}

static void
cc_dock_panel_class_init (CcDockPanelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = cc_dock_panel_dispose;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/dock/cc-dock-panel.ui");

  gtk_widget_class_bind_template_child (widget_class, CcDockPanel, dock_autohide_switch);
  gtk_widget_class_bind_template_child (widget_class, CcDockPanel, icon_size_32);
  gtk_widget_class_bind_template_child (widget_class, CcDockPanel, icon_size_48);
  gtk_widget_class_bind_template_child (widget_class, CcDockPanel, icon_size_64);
  gtk_widget_class_bind_template_child (widget_class, CcDockPanel, icon_size_scale);

  gtk_widget_class_bind_template_callback (widget_class, on_view_dock_settings_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_icon_size_32_toggled);
  gtk_widget_class_bind_template_callback (widget_class, on_icon_size_48_toggled);
  gtk_widget_class_bind_template_callback (widget_class, on_icon_size_64_toggled);
}

static void
cc_dock_panel_init (CcDockPanel *self)
{
  g_autoptr(GSettingsSchema) schema = NULL;

  g_resources_register (cc_dock_get_resource ());

  gtk_widget_init_template (GTK_WIDGET (self));
  /* Only load if we have dash to dock installed */
  schema = g_settings_schema_source_lookup (g_settings_schema_source_get_default (), "org.gnome.shell.extensions.dash-to-dock", TRUE);
  if (!schema)
    {
      g_warning ("No dock is installed here. Panel disabled. Please fix your installation.");
      return;
    }

  self->dock_settings = g_settings_new_full (schema, NULL, NULL);
  g_signal_connect_object (self->dock_settings, "changed::" ICONSIZE_KEY,
                           G_CALLBACK (icon_size_widget_refresh), self, G_CONNECT_SWAPPED);

  g_settings_bind (self->dock_settings, "dock-position",
                   self->dock_position_combo, "selected",
                   G_SETTINGS_BIND_DEFAULT);
  
  g_settings_bind (self->dock_settings, "dock-fixed",
                   self->dock_autohide_switch, "active",
                   G_SETTINGS_BIND_INVERT_BOOLEAN);

  g_dbus_proxy_new_for_bus (G_BUS_TYPE_SESSION,
                            G_DBUS_PROXY_FLAGS_NONE,
                            NULL,
                            "org.gnome.Shell.Extensions",
                            "/org/gnome/Shell/Extensions",
                            "org.gnome.Shell.Extensions",
                            NULL,
                            got_extension_proxy_cb,
                            self);
}