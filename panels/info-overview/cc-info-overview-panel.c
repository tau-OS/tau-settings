/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 *
 * Copyright (C) 2019 Purism SPC
 * Copyright (C) 2017 Mohammed Sadiq <sadiq@sadiqpk.org>
 * Copyright (C) 2010 Red Hat, Inc
 * Copyright (C) 2008 William Jon McCann <jmccann@redhat.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <config.h>

#include "cc-hostname-entry.h"

#include "cc-info-overview-resources.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <gio/gio.h>
#include <gio/gunixmounts.h>
#include <gio/gdesktopappinfo.h>

#include <gdk/gdk.h>

#ifdef GDK_WINDOWING_WAYLAND
#include <gdk/wayland/gdkwayland.h>
#endif
#ifdef GDK_WINDOWING_X11
#include <gdk/x11/gdkx.h>
#endif

#include "cc-list-row.h"
#include "cc-info-overview-panel.h"

struct _CcInfoOverviewPanel
{
  CcPanel          parent_instance;

  GtkEntry        *device_name_entry;
  GtkWidget       *rename_button;
  CcListRow       *gnome_version_row;
  GtkDialog       *hostname_editor;
  CcHostnameEntry *hostname_entry;
  CcListRow       *hostname_row;
  GtkPicture      *os_logo;
  CcListRow       *os_name_row;
  CcListRow       *software_updates_row;
  CcListRow       *virtualization_row;
  CcListRow       *windowing_system_row;
};

typedef struct
{
  char *major;
  char *minor;
  char *distributor;
  char *date;
  char **current;
} VersionData;

static void
version_data_free (VersionData *data)
{
  g_free (data->major);
  g_free (data->minor);
  g_free (data->distributor);
  g_free (data->date);
  g_free (data);
}

G_DEFINE_AUTOPTR_CLEANUP_FUNC (VersionData, version_data_free);

G_DEFINE_TYPE (CcInfoOverviewPanel, cc_info_overview_panel, CC_TYPE_PANEL)

static void
version_start_element_handler (GMarkupParseContext      *ctx,
                               const char               *element_name,
                               const char              **attr_names,
                               const char              **attr_values,
                               gpointer                  user_data,
                               GError                  **error)
{
  VersionData *data = user_data;
  if (g_str_equal (element_name, "platform"))
    data->current = &data->major;
  else if (g_str_equal (element_name, "minor"))
    data->current = &data->minor;
  else if (g_str_equal (element_name, "distributor"))
    data->current = &data->distributor;
  else if (g_str_equal (element_name, "date"))
    data->current = &data->date;
  else
    data->current = NULL;
}

static void
version_end_element_handler (GMarkupParseContext      *ctx,
                             const char               *element_name,
                             gpointer                  user_data,
                             GError                  **error)
{
  VersionData *data = user_data;
  data->current = NULL;
}

static void
version_text_handler (GMarkupParseContext *ctx,
                      const char          *text,
                      gsize                text_len,
                      gpointer             user_data,
                      GError             **error)
{
  VersionData *data = user_data;
  if (data->current != NULL)
    {
      g_autofree char *stripped = NULL;

      stripped = g_strstrip (g_strdup (text));
      g_free (*data->current);
      *data->current = g_strdup (stripped);
    }
}

static gboolean
load_gnome_version (char **version,
                    char **distributor,
                    char **date)
{
  GMarkupParser version_parser = {
    version_start_element_handler,
    version_end_element_handler,
    version_text_handler,
    NULL,
    NULL,
  };
  g_autoptr(GError) error = NULL;
  g_autoptr(GMarkupParseContext) ctx = NULL;
  g_autofree char *contents = NULL;
  gsize length;
  g_autoptr(VersionData) data = NULL;

  if (!g_file_get_contents (DATADIR "/gnome/gnome-version.xml",
                            &contents,
                            &length,
                            &error))
    return FALSE;

  data = g_new0 (VersionData, 1);
  ctx = g_markup_parse_context_new (&version_parser, 0, data, NULL);

  if (!g_markup_parse_context_parse (ctx, contents, length, &error))
    {
      g_warning ("Invalid version file: '%s'", error->message);
    }
  else
    {
      if (version != NULL)
        *version = g_strdup_printf ("%s.%s", data->major, data->minor);
      if (distributor != NULL)
        *distributor = g_strdup (data->distributor);
      if (date != NULL)
        *date = g_strdup (data->date);

      return TRUE;
    }

  return FALSE;
};

static char *
get_os_name (void)
{
  g_autofree gchar *name = NULL;
  g_autofree gchar *version_id = NULL;
  g_autofree gchar *pretty_name = NULL;
  g_autofree gchar *build_id = NULL;
  g_autofree gchar *name_version = NULL;
  g_autofree gchar *codename = NULL;
  g_autofree gchar *version = NULL;
  gchar *result = NULL;

  name = g_get_os_info (G_OS_INFO_KEY_NAME);
  version_id = g_get_os_info (G_OS_INFO_KEY_VERSION_ID);
  pretty_name = g_get_os_info (G_OS_INFO_KEY_PRETTY_NAME);
  build_id = g_get_os_info ("BUILD_ID");
  codename = g_get_os_info (G_OS_INFO_KEY_VERSION_CODENAME);
  version = g_get_os_info (G_OS_INFO_KEY_VERSION);

  if (codename && version)
    name_version = g_strdup_printf ("%s (%s)", version, codename);
  else if (pretty_name)
    name_version = g_strdup (pretty_name);
  else if (name && version_id)
    name_version = g_strdup_printf ("%s %s", name, version_id);
  else
    name_version = g_strdup (_("Unknown"));

  if (build_id)
    {
      /* translators: This is the name of the OS, followed by the build ID, for
       * example:
       * "Fedora 25 (Workstation Edition); Build ID: xyz" or
       * "Ubuntu 16.04 LTS; Build ID: jki" */
      result = g_strdup_printf (_("%s; Build ID: %s"), name_version, build_id);
    }
  else
    {
      result = g_strdup (name_version);
    }

  return result;
}

static struct {
  const char *id;
  const char *display;
} const virt_tech[] = {
  { "kvm", "KVM" },
  { "qemu", "QEmu" },
  { "vmware", "VMware" },
  { "microsoft", "Microsoft" },
  { "oracle", "Oracle" },
  { "xen", "Xen" },
  { "bochs", "Bochs" },
  { "chroot", "chroot" },
  { "openvz", "OpenVZ" },
  { "lxc", "LXC" },
  { "lxc-libvirt", "LXC (libvirt)" },
  { "systemd-nspawn", "systemd (nspawn)" }
};

static void
set_virtualization_label (CcInfoOverviewPanel *self,
                          const char          *virt)
{
  const char *display_name;
  guint i;

  if (virt == NULL || *virt == '\0')
    {
      gtk_widget_hide (GTK_WIDGET (self->virtualization_row));
      return;
    }

  gtk_widget_show (GTK_WIDGET (self->virtualization_row));

  display_name = NULL;
  for (i = 0; i < G_N_ELEMENTS (virt_tech); i++)
    {
      if (g_str_equal (virt_tech[i].id, virt))
        {
          display_name = _(virt_tech[i].display);
          break;
        }
    }

  cc_list_row_set_secondary_label (self->virtualization_row, display_name ? display_name : virt);
}

static void
info_overview_panel_setup_virt (CcInfoOverviewPanel *self)
{
  g_autoptr(GError) error = NULL;
  g_autoptr(GDBusProxy) systemd_proxy = NULL;
  g_autoptr(GVariant) variant = NULL;
  GVariant *inner;

  systemd_proxy = g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SYSTEM,
                                                 G_DBUS_PROXY_FLAGS_NONE,
                                                 NULL,
                                                 "org.freedesktop.systemd1",
                                                 "/org/freedesktop/systemd1",
                                                 "org.freedesktop.systemd1",
                                                 NULL,
                                                 &error);

  if (systemd_proxy == NULL)
    {
      g_debug ("systemd not available, bailing: %s", error->message);
      set_virtualization_label (self, NULL);
      return;
    }

  variant = g_dbus_proxy_call_sync (systemd_proxy,
                                    "org.freedesktop.DBus.Properties.Get",
                                    g_variant_new ("(ss)", "org.freedesktop.systemd1.Manager", "Virtualization"),
                                    G_DBUS_CALL_FLAGS_NONE,
                                    -1,
                                    NULL,
                                    &error);
  if (variant == NULL)
    {
      g_debug ("Failed to get property '%s': %s", "Virtualization", error->message);
      set_virtualization_label (self, NULL);
      return;
    }

  g_variant_get (variant, "(v)", &inner);
  set_virtualization_label (self, g_variant_get_string (inner, NULL));
}

static const char *
get_windowing_system (void)
{
  GdkDisplay *display;

  display = gdk_display_get_default ();

#if defined(GDK_WINDOWING_X11)
  if (GDK_IS_X11_DISPLAY (display))
    return _("X11");
#endif /* GDK_WINDOWING_X11 */
#if defined(GDK_WINDOWING_WAYLAND)
  if (GDK_IS_WAYLAND_DISPLAY (display))
    return _("Wayland");
#endif /* GDK_WINDOWING_WAYLAND */
  return C_("Windowing system (Wayland, X11, or Unknown)", "Unknown");
}

static char *
get_gnome_version (GDBusProxy *proxy)
{
  g_autoptr(GVariant) variant = NULL;
  const char *gnome_version = NULL;
  if (!proxy)
    return NULL;

  variant = g_dbus_proxy_get_cached_property (proxy, "ShellVersion");
  if (!variant)
    return NULL;

  gnome_version = g_variant_get_string (variant, NULL);
  if (!gnome_version || *gnome_version == '\0')
    return NULL;
  return g_strdup (gnome_version);
}

static void
shell_proxy_ready (GObject             *source,
                   GAsyncResult        *res,
                   CcInfoOverviewPanel *self)
{
  g_autoptr(GDBusProxy) proxy = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(GVariant) variant = NULL;
  g_autofree char *gnome_version = NULL;

  proxy = cc_object_storage_create_dbus_proxy_finish (res, &error);
  if (!proxy)
    {
      if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        return;
      g_warning ("Failed to contact gnome-shell: %s", error->message);
    }

  gnome_version = get_gnome_version (proxy);

  if (!gnome_version)
    {
      /* translators: this is the placeholder string when the GNOME Shell
       * version couldn't be loaded, eg. “GNOME Version: Not Available” */
      cc_list_row_set_secondary_label (self->gnome_version_row, _("Not Available"));
    }
  else
    {
      cc_list_row_set_secondary_label (self->gnome_version_row, gnome_version);
    }
}


static void
info_overview_panel_setup_overview (CcInfoOverviewPanel *self)
{
  g_autofree gchar *gnome_version = NULL;
  g_autofree char *os_name_text = NULL;

  if (load_gnome_version (&gnome_version, NULL, NULL))
    cc_list_row_set_secondary_label (self->gnome_version_row, gnome_version);

  cc_list_row_set_secondary_label (self->windowing_system_row, get_windowing_system ());

  os_name_text = get_os_name ();
  cc_list_row_set_secondary_label (self->os_name_row, os_name_text);
}

static gboolean
does_gnome_software_allow_updates (void)
{
  const gchar *schema_id  = "org.gnome.software";
  GSettingsSchemaSource *source;
  g_autoptr(GSettingsSchema) schema = NULL;
  g_autoptr(GSettings) settings = NULL;

  source = g_settings_schema_source_get_default ();

  if (source == NULL)
    return FALSE;

  schema = g_settings_schema_source_lookup (source, schema_id, FALSE);

  if (schema == NULL)
    return FALSE;

  settings = g_settings_new (schema_id);
  return g_settings_get_boolean (settings, "allow-updates");
}

static gboolean
does_gnome_software_exist (void)
{
  g_autofree gchar *path = g_find_program_in_path ("gnome-software");
  return path != NULL;
}

static gboolean
does_gpk_update_viewer_exist (void)
{
  g_autofree gchar *path = g_find_program_in_path ("gpk-update-viewer");
  return path != NULL;
}

static void
open_software_update (CcInfoOverviewPanel *self)
{
  g_autoptr(GError) error = NULL;
  gboolean ret;
  char *argv[3];

  if (does_gnome_software_exist ())
    {
      argv[0] = "gnome-software";
      argv[1] = "--mode=updates";
      argv[2] = NULL;
    }
  else
    {
      argv[0] = "gpk-update-viewer";
      argv[1] = NULL;
    }
  ret = g_spawn_async (NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, &error);
  if (!ret)
      g_warning ("Failed to spawn %s: %s", argv[0], error->message);
}

static void
on_device_name_entry_changed (CcInfoOverviewPanel *self)
{
  const gchar *current_hostname, *new_hostname;

  current_hostname = gtk_editable_get_text (GTK_EDITABLE (self->hostname_entry));
  new_hostname = gtk_editable_get_text (GTK_EDITABLE (self->device_name_entry));
  gtk_widget_set_sensitive (self->rename_button,
                            g_strcmp0 (current_hostname, new_hostname) != 0);
}

static void
update_device_name (CcInfoOverviewPanel *self)
{
  const gchar *hostname;

  /* We simply change the CcHostnameEntry text. CcHostnameEntry
   * listens to changes and updates hostname on change.
   */
  hostname = gtk_editable_get_text (GTK_EDITABLE (self->device_name_entry));
  gtk_editable_set_text (GTK_EDITABLE (self->hostname_entry), hostname);
}

static void
on_hostname_editor_dialog_response_cb (GtkDialog           *dialog,
                                       gint                 response,
                                       CcInfoOverviewPanel *self)
{
  if (response == GTK_RESPONSE_APPLY)
    {
      update_device_name (self);
    }

  gtk_window_close (GTK_WINDOW (dialog));
}

static void
on_device_name_entry_activated_cb (CcInfoOverviewPanel *self)
{
  update_device_name (self);
  gtk_window_close (GTK_WINDOW (self->hostname_editor));
}

static void
open_hostname_edit_dialog (CcInfoOverviewPanel *self)
{
  GtkWindow *toplevel;
  CcShell *shell;
  const gchar *hostname;

  g_assert (CC_IS_INFO_OVERVIEW_PANEL (self));

  shell = cc_panel_get_shell (CC_PANEL (self));
  toplevel = GTK_WINDOW (cc_shell_get_toplevel (shell));
  gtk_window_set_transient_for (GTK_WINDOW (self->hostname_editor), toplevel);

  hostname = gtk_editable_get_text (GTK_EDITABLE (self->hostname_entry));
  gtk_editable_set_text (GTK_EDITABLE (self->device_name_entry), hostname);
  gtk_widget_grab_focus (GTK_WIDGET (self->device_name_entry));

  gtk_window_present (GTK_WINDOW (self->hostname_editor));

}

static void
cc_info_panel_row_activated_cb (CcInfoOverviewPanel *self,
                                CcListRow           *row)
{
  g_assert (CC_IS_INFO_OVERVIEW_PANEL (self));
  g_assert (CC_IS_LIST_ROW (row));

  if (row == self->hostname_row)
    open_hostname_edit_dialog (self);
  else if (row == self->software_updates_row)
    open_software_update (self);
}

#if !defined(DISTRIBUTOR_LOGO) || defined(DARK_MODE_DISTRIBUTOR_LOGO)
static gboolean
use_dark_theme (CcInfoOverviewPanel *panel)
{
  AdwStyleManager *style_manager = adw_style_manager_get_default ();

  return adw_style_manager_get_dark (style_manager);
}
#endif

static void
setup_os_logo (CcInfoOverviewPanel *panel)
{
#ifdef DISTRIBUTOR_LOGO
#ifdef DARK_MODE_DISTRIBUTOR_LOGO
  if (use_dark_theme (panel))
    {
      gtk_picture_set_filename (panel->os_logo, DARK_MODE_DISTRIBUTOR_LOGO);
      return;
    }
#endif
  gtk_picture_set_filename (panel->os_logo, DISTRIBUTOR_LOGO);
  return;
#else
  GtkIconTheme *icon_theme;
  g_autofree char *logo_name = g_get_os_info ("LOGO");
  g_autoptr(GtkIconPaintable) icon_paintable = NULL;
  g_autoptr(GPtrArray) array = NULL;
  g_autoptr(GIcon) icon = NULL;
  gboolean dark;

  dark = use_dark_theme (panel);
  if (logo_name == NULL)
    logo_name = g_strdup ("gnome-logo");

  array = g_ptr_array_new_with_free_func (g_free);
  if (dark)
    g_ptr_array_add (array, (gpointer) g_strdup_printf ("%s-text-dark", logo_name));
  g_ptr_array_add (array, (gpointer) g_strdup_printf ("%s-text", logo_name));
  if (dark)
    g_ptr_array_add (array, (gpointer) g_strdup_printf ("%s-dark", logo_name));
  g_ptr_array_add (array, (gpointer) g_strdup_printf ("%s", logo_name));

  icon = g_themed_icon_new_from_names ((char **) array->pdata, array->len);
  icon_theme = gtk_icon_theme_get_for_display (gdk_display_get_default ());
  icon_paintable = gtk_icon_theme_lookup_by_gicon (icon_theme, icon,
                                                   192,
                                                   gtk_widget_get_scale_factor (GTK_WIDGET (panel)),
                                                   gtk_widget_get_direction (GTK_WIDGET (panel)),
                                                   0);
  gtk_picture_set_paintable (panel->os_logo, GDK_PAINTABLE (icon_paintable));
#endif
}

static void
cc_info_overview_panel_class_init (CcInfoOverviewPanelClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/info-overview/cc-info-overview-panel.ui");

  gtk_widget_class_bind_template_child (widget_class, CcInfoOverviewPanel, device_name_entry);
  gtk_widget_class_bind_template_child (widget_class, CcInfoOverviewPanel, gnome_version_row);
  gtk_widget_class_bind_template_child (widget_class, CcInfoOverviewPanel, hostname_editor);
  gtk_widget_class_bind_template_child (widget_class, CcInfoOverviewPanel, hostname_entry);
  gtk_widget_class_bind_template_child (widget_class, CcInfoOverviewPanel, hostname_row);
  gtk_widget_class_bind_template_child (widget_class, CcInfoOverviewPanel, os_logo);
  gtk_widget_class_bind_template_child (widget_class, CcInfoOverviewPanel, os_name_row);
  gtk_widget_class_bind_template_child (widget_class, CcInfoOverviewPanel, rename_button);
  gtk_widget_class_bind_template_child (widget_class, CcInfoOverviewPanel, software_updates_row);
  gtk_widget_class_bind_template_child (widget_class, CcInfoOverviewPanel, virtualization_row);
  gtk_widget_class_bind_template_child (widget_class, CcInfoOverviewPanel, windowing_system_row);

  gtk_widget_class_bind_template_callback (widget_class, cc_info_panel_row_activated_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_device_name_entry_changed);
  gtk_widget_class_bind_template_callback (widget_class, on_device_name_entry_activated_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_hostname_editor_dialog_response_cb);

  g_type_ensure (CC_TYPE_LIST_ROW);
  g_type_ensure (CC_TYPE_HOSTNAME_ENTRY);
}

static void
cc_info_overview_panel_init (CcInfoOverviewPanel *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  g_resources_register (cc_info_overview_get_resource ());

  if ((!does_gnome_software_exist () || !does_gnome_software_allow_updates ()) && !does_gpk_update_viewer_exist ())
    gtk_widget_hide (GTK_WIDGET (self->software_updates_row));

  info_overview_panel_setup_overview (self);
  info_overview_panel_setup_virt (self);

  setup_os_logo (self);
}

GtkWidget *
cc_info_overview_panel_new (void)
{
  return g_object_new (CC_TYPE_INFO_OVERVIEW_PANEL,
                       NULL);
}
