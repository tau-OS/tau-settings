/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 *
 * Copyright (C) 2019 Purism SPC
 * Copyright (C) 2017 Mohammed Sadiq <sadiq@sadiqpk.org>
 * Copyright (C) 2010 Red Hat, Inc
 * Copyright (C) 2008 William Jon McCann <jmccann@redhat.com>
 * Copyright (C) 2022 Fyra Labs
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

#include "cc-system-resources.h"
#include "info-cleanup.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <gio/gio.h>
#include <gio/gunixmounts.h>
#include <gio/gdesktopappinfo.h>

#include <glibtop/fsusage.h>
#include <glibtop/mountlist.h>
#include <glibtop/mem.h>
#include <glibtop/sysinfo.h>
#include <udisks/udisks.h>
#include <gudev/gudev.h>

#include <gdk/gdk.h>

#include "cc-list-row.h"
#include "cc-system-panel.h"

struct _CcSystemPanel
{
  CcPanel parent_instance;

  GKeyFile *oem_information;
  gboolean *oem_information_valid;

  CcListRow *disk_row;
  CcListRow *graphics_row;
  CcListRow *hardware_model_row;
  CcListRow *homepage_row;
  CcListRow *manufacturer_row;
  CcListRow *memory_row;
  GtkPicture *os_logo;
  CcListRow *os_type_row;
  CcListRow *processor_row;
  CcListRow *support_row;
};

G_DEFINE_TYPE(CcSystemPanel, cc_system_panel, CC_TYPE_PANEL)

static void
load_oem_information(CcSystemPanel *self)
{
  g_autoptr(GError) oem_error = NULL;
  g_autofree gchar *dir = NULL;
  dir = g_build_filename(DATADIR, "oem", "oem.conf", NULL);

  g_key_file_load_from_file(self->oem_information, dir,
                            G_KEY_FILE_NONE, &oem_error);

  if (oem_error && !g_error_matches(oem_error, G_FILE_ERROR, G_FILE_ERROR_NOENT))
  {
    g_warning("Could not load OEM Information '%s': %s",
              DATADIR "/oem/oem.conf", oem_error->message);
  }
}

static char *
get_renderer_from_session(void)
{
  g_autoptr(GDBusProxy) session_proxy = NULL;
  g_autoptr(GVariant) renderer_variant = NULL;
  char *renderer;
  g_autoptr(GError) error = NULL;

  session_proxy = g_dbus_proxy_new_for_bus_sync(G_BUS_TYPE_SESSION,
                                                G_DBUS_PROXY_FLAGS_NONE,
                                                NULL,
                                                "org.gnome.SessionManager",
                                                "/org/gnome/SessionManager",
                                                "org.gnome.SessionManager",
                                                NULL, &error);
  if (error != NULL)
  {
    g_warning("Unable to connect to create a proxy for org.gnome.SessionManager: %s",
              error->message);
    return NULL;
  }

  renderer_variant = g_dbus_proxy_get_cached_property(session_proxy, "Renderer");

  if (!renderer_variant)
  {
    g_warning("Unable to retrieve org.gnome.SessionManager.Renderer property");
    return NULL;
  }

  renderer = info_cleanup(g_variant_get_string(renderer_variant, NULL));

  return renderer;
}

/* @env is an array of strings with each pair of strings being the
 * key followed by the value */
static char *
get_renderer_from_helper(const char **env)
{
  int status;
  char *argv[] = {LIBEXECDIR "/gnome-control-center-print-renderer", NULL};
  g_auto(GStrv) envp = NULL;
  g_autofree char *renderer = NULL;
  g_autoptr(GError) error = NULL;

  g_debug("About to launch '%s'", argv[0]);

  if (env != NULL)
  {
    guint i;
    g_debug("With environment:");
    envp = g_get_environ();
    for (i = 0; env != NULL && env[i] != NULL; i = i + 2)
    {
      g_debug("  %s = %s", env[i], env[i + 1]);
      envp = g_environ_setenv(envp, env[i], env[i + 1], TRUE);
    }
  }
  else
  {
    g_debug("No additional environment variables");
  }

  if (!g_spawn_sync(NULL, (char **)argv, envp, 0, NULL, NULL, &renderer, NULL, &status, &error))
  {
    g_debug("Failed to get GPU: %s", error->message);
    return NULL;
  }

  if (!g_spawn_check_wait_status(status, NULL))
    return NULL;

  if (renderer == NULL || *renderer == '\0')
    return NULL;

  return info_cleanup(renderer);
}

typedef struct
{
  char *name;
  gboolean is_default;
} GpuData;

static int
gpu_data_sort(gconstpointer a, gconstpointer b)
{
  GpuData *gpu_a = (GpuData *)a;
  GpuData *gpu_b = (GpuData *)b;

  if (gpu_a->is_default)
    return 1;
  if (gpu_b->is_default)
    return -1;
  return 0;
}

static void
gpu_data_free(GpuData *data)
{
  g_free(data->name);
  g_free(data);
}

static char *
get_renderer_from_switcheroo(void)
{
  g_autoptr(GDBusProxy) switcheroo_proxy = NULL;
  g_autoptr(GVariant) variant = NULL;
  g_autoptr(GError) error = NULL;
  GString *renderers_string;
  guint i, num_children;
  GSList *renderers, *l;

  switcheroo_proxy = g_dbus_proxy_new_for_bus_sync(G_BUS_TYPE_SYSTEM,
                                                   G_DBUS_PROXY_FLAGS_NONE,
                                                   NULL,
                                                   "net.hadess.SwitcherooControl",
                                                   "/net/hadess/SwitcherooControl",
                                                   "net.hadess.SwitcherooControl",
                                                   NULL, &error);
  if (switcheroo_proxy == NULL)
  {
    g_debug("Unable to connect to create a proxy for net.hadess.SwitcherooControl: %s",
            error->message);
    return NULL;
  }

  variant = g_dbus_proxy_get_cached_property(switcheroo_proxy, "GPUs");

  if (!variant)
  {
    g_debug("Unable to retrieve net.hadess.SwitcherooControl.GPUs property, the daemon is likely not running");
    return NULL;
  }

  renderers_string = g_string_new(NULL);
  num_children = g_variant_n_children(variant);
  renderers = NULL;
  for (i = 0; i < num_children; i++)
  {
    g_autoptr(GVariant) gpu;
    g_autoptr(GVariant) name = NULL;
    g_autoptr(GVariant) env = NULL;
    g_autoptr(GVariant) default_variant = NULL;
    const char *name_s;
    g_autofree const char **env_s = NULL;
    gsize env_len;
    g_autofree char *renderer = NULL;
    GpuData *gpu_data;

    gpu = g_variant_get_child_value(variant, i);
    if (!gpu ||
        !g_variant_is_of_type(gpu, G_VARIANT_TYPE("a{s*}")))
      continue;

    name = g_variant_lookup_value(gpu, "Name", NULL);
    env = g_variant_lookup_value(gpu, "Environment", NULL);
    if (!name || !env)
      continue;
    name_s = g_variant_get_string(name, NULL);
    g_debug("Getting renderer from helper for GPU '%s'", name_s);
    env_s = g_variant_get_strv(env, &env_len);
    if (env_s != NULL && env_len % 2 != 0)
    {
      g_autofree char *debug = NULL;
      debug = g_strjoinv("\n", (char **)env_s);
      g_warning("Invalid environment returned from switcheroo:\n%s", debug);
      g_clear_pointer(&env_s, g_free);
    }

    renderer = get_renderer_from_helper(env_s);
    default_variant = g_variant_lookup_value(gpu, "Default", NULL);

    /* We could give up if we don't have a renderer, but that
     * might just mean gnome-session isn't installed. We fall back
     * to the device name in udev instead, which is better than nothing */

    gpu_data = g_new0(GpuData, 1);
    gpu_data->name = g_strdup(renderer ? renderer : name_s);
    gpu_data->is_default = default_variant ? g_variant_get_boolean(default_variant) : FALSE;
    renderers = g_slist_prepend(renderers, gpu_data);
  }

  renderers = g_slist_sort(renderers, gpu_data_sort);
  for (l = renderers; l != NULL; l = l->next)
  {
    GpuData *data = l->data;
    if (renderers_string->len > 0)
      g_string_append(renderers_string, " / ");
    g_string_append(renderers_string, data->name);
  }
  g_slist_free_full(renderers, (GDestroyNotify)gpu_data_free);

  if (renderers_string->len == 0)
  {
    g_string_free(renderers_string, TRUE);
    return NULL;
  }

  return g_string_free(renderers_string, FALSE);
}

static gchar *
get_graphics_hardware_string(void)
{
  g_autofree char *discrete_renderer = NULL;
  g_autofree char *renderer = NULL;

  renderer = get_renderer_from_switcheroo();
  if (!renderer)
    renderer = get_renderer_from_session();
  if (!renderer)
    renderer = get_renderer_from_helper(NULL);
  if (!renderer)
    return g_strdup(_("Unknown"));
  return g_strdup(renderer);
}

static char *
get_os_type(void)
{
  if (GLIB_SIZEOF_VOID_P == 8)
    /* translators: This is the type of architecture for the OS */
    return g_strdup_printf(_("64-bit"));
  else
    /* translators: This is the type of architecture for the OS */
    return g_strdup_printf(_("32-bit"));
}

static void
get_manufacturer(CcSystemPanel *self)
{
  const char *manufacturer_string;
  g_autoptr(GError) error = NULL;

  manufacturer_string = g_key_file_get_string(self->oem_information, "OEM", "Manufacturer", &error);
  if (!manufacturer_string)
  {
    g_debug("Unable to retrieve manufacturer from OEM configuration");
    return;
  }

  if (manufacturer_string && g_strcmp0(manufacturer_string, "") != 0)
  {
    cc_list_row_set_secondary_label(self->manufacturer_row, manufacturer_string);
    gtk_widget_set_visible(GTK_WIDGET(self->manufacturer_row), TRUE);
  }
}

static void
get_primary_disc_info(CcSystemPanel *self)
{
  g_autoptr(UDisksClient) client = NULL;
  GDBusObjectManager *manager;
  g_autolist(GDBusObject) objects = NULL;
  GList *l;
  guint64 total_size;
  g_autoptr(GError) error = NULL;
  total_size = 0;

  client = udisks_client_new_sync(NULL, &error);
  if (client == NULL)
  {
    g_warning("Unable to get UDisks client: %s. Disk information will not be available.",
              error->message);
    cc_list_row_set_secondary_label(self->disk_row, _("Unknown"));
    return;
  }

  manager = udisks_client_get_object_manager(client);
  objects = g_dbus_object_manager_get_objects(manager);
  for (l = objects; l != NULL; l = l->next)
  {
    UDisksDrive *drive;
    drive = udisks_object_peek_drive(UDISKS_OBJECT(l->data));

    /* Skip removable devices */
    if (drive == NULL ||
        udisks_drive_get_removable(drive) ||
        udisks_drive_get_ejectable(drive))
    {
      continue;
    }

    total_size += udisks_drive_get_size(drive);
  }

  if (total_size > 0)
  {
    g_autofree gchar *size = g_format_size(total_size);
    cc_list_row_set_secondary_label(self->disk_row, size);
  }
  else
  {
    cc_list_row_set_secondary_label(self->disk_row, _("Unknown"));
  }
}

static void
get_hardware_model(CcSystemPanel *self)
{
  g_autoptr(GError) error = NULL;

  if (g_key_file_has_key(self->oem_information, "OEM", "Product", &error))
  {
    const char *product_string, *version_string;

    product_string = g_key_file_get_string(self->oem_information, "OEM", "Product", &error);
    version_string = g_key_file_get_string(self->oem_information, "OEM", "Version", &error);
    if (!product_string)
    {
      g_debug("Unable to retrieve product from OEM configuration");
      return;
    }
    if (!version_string)
    {
      g_debug("Unable to retrieve product version from OEM configuration");
    }

    if (product_string && g_strcmp0(product_string, "") != 0)
    {
      g_autofree gchar *vendor_model = NULL;

      if (version_string)
      {
        vendor_model = g_strdup_printf("%s %s", product_string, version_string);
      }
      else
      {
        vendor_model = g_strdup_printf("%s", product_string);
      }

      cc_list_row_set_secondary_label(self->hardware_model_row, vendor_model);
      gtk_widget_set_visible(GTK_WIDGET(self->hardware_model_row), TRUE);
    }
  }
  else
  {
    g_autoptr(GDBusProxy) hostnamed_proxy = NULL;
    g_autoptr(GVariant) vendor_variant = NULL;
    g_autoptr(GVariant) model_variant = NULL;
    const char *vendor_string, *model_string;
    g_autoptr(GError) error = NULL;

    hostnamed_proxy = g_dbus_proxy_new_for_bus_sync(G_BUS_TYPE_SYSTEM,
                                                    G_DBUS_PROXY_FLAGS_NONE,
                                                    NULL,
                                                    "org.freedesktop.hostname1",
                                                    "/org/freedesktop/hostname1",
                                                    "org.freedesktop.hostname1",
                                                    NULL,
                                                    &error);
    if (hostnamed_proxy == NULL)
    {
      g_debug("Couldn't get hostnamed to start, bailing: %s", error->message);
      return;
    }

    vendor_variant = g_dbus_proxy_get_cached_property(hostnamed_proxy, "HardwareVendor");
    if (!vendor_variant)
    {
      g_debug("Unable to retrieve org.freedesktop.hostname1.HardwareVendor property");
      return;
    }

    model_variant = g_dbus_proxy_get_cached_property(hostnamed_proxy, "HardwareModel");
    if (!model_variant)
    {
      g_debug("Unable to retrieve org.freedesktop.hostname1.HardwareModel property");
      return;
    }

    vendor_string = g_variant_get_string(vendor_variant, NULL),
    model_string = g_variant_get_string(model_variant, NULL);

    if (vendor_string && g_strcmp0(vendor_string, "") != 0)
    {
      g_autofree gchar *vendor_model = NULL;

      vendor_model = g_strdup_printf("%s %s", vendor_string, model_string);

      cc_list_row_set_secondary_label(self->hardware_model_row, vendor_model);
      gtk_widget_set_visible(GTK_WIDGET(self->hardware_model_row), TRUE);
    }
  }
}

static char *
get_cpu_info(const glibtop_sysinfo *info)
{
  g_autoptr(GHashTable) counts = NULL;
  g_autoptr(GString) cpu = NULL;
  GHashTableIter iter;
  gpointer key, value;
  int i;
  int j;

  counts = g_hash_table_new(g_str_hash, g_str_equal);

  /* count duplicates */
  for (i = 0; i != info->ncpu; ++i)
  {
    const char *const keys[] = {"model name", "cpu", "Processor"};
    char *model;
    int *count;

    model = NULL;

    for (j = 0; model == NULL && j != G_N_ELEMENTS(keys); ++j)
    {
      model = g_hash_table_lookup(info->cpuinfo[i].values,
                                  keys[j]);
    }

    if (model == NULL)
      continue;

    count = g_hash_table_lookup(counts, model);
    if (count == NULL)
      g_hash_table_insert(counts, model, GINT_TO_POINTER(1));
    else
      g_hash_table_replace(counts, model, GINT_TO_POINTER(GPOINTER_TO_INT(count) + 1));
  }

  cpu = g_string_new(NULL);
  g_hash_table_iter_init(&iter, counts);
  while (g_hash_table_iter_next(&iter, &key, &value))
  {
    g_autofree char *cleanedup = NULL;
    int count;

    count = GPOINTER_TO_INT(value);
    cleanedup = info_cleanup((const char *)key);
    if (cpu->len != 0)
      g_string_append_printf(cpu, " ");
    if (count > 1)
      g_string_append_printf(cpu, "%s \303\227 %d", cleanedup, count);
    else
      g_string_append_printf(cpu, "%s", cleanedup);
  }

  return g_strdup(cpu->str);
}

static guint64
get_ram_size_libgtop(void)
{
  glibtop_mem mem;

  glibtop_get_mem(&mem);
  return mem.total;
}

static guint64
get_ram_size_dmi(void)
{
  g_autoptr(GUdevClient) client = NULL;
  g_autoptr(GUdevDevice) dmi = NULL;
  const gchar *const subsystems[] = {"dmi", NULL};
  guint64 ram_total = 0;
  guint64 num_ram;
  guint i;

  client = g_udev_client_new(subsystems);
  dmi = g_udev_client_query_by_sysfs_path(client, "/sys/devices/virtual/dmi/id");
  if (!dmi)
    return 0;
  num_ram = g_udev_device_get_property_as_uint64(dmi, "MEMORY_ARRAY_NUM_DEVICES");
  for (i = 0; i < num_ram; i++)
  {
    g_autofree char *prop = NULL;

    prop = g_strdup_printf("MEMORY_DEVICE_%d_SIZE", i);
    ram_total += g_udev_device_get_property_as_uint64(dmi, prop);
  }
  return ram_total;
}

static void
system_panel_setup_overview(CcSystemPanel *self)
{
  guint64 ram_size;
  const glibtop_sysinfo *info;
  g_autofree char *memory_text = NULL;
  g_autofree char *cpu_text = NULL;
  g_autofree char *os_type_text = NULL;
  g_autofree gchar *graphics_hardware_string = NULL;

  get_hardware_model(self);
  get_manufacturer(self);

  ram_size = get_ram_size_dmi();
  if (ram_size == 0)
    ram_size = get_ram_size_libgtop();
  memory_text = g_format_size_full(ram_size, G_FORMAT_SIZE_IEC_UNITS);
  cc_list_row_set_secondary_label(self->memory_row, memory_text);

  info = glibtop_get_sysinfo();

  cpu_text = get_cpu_info(info);
  cc_list_row_set_secondary_markup(self->processor_row, cpu_text);

  os_type_text = get_os_type();
  cc_list_row_set_secondary_label(self->os_type_row, os_type_text);

  get_primary_disc_info(self);

  graphics_hardware_string = get_graphics_hardware_string();
  cc_list_row_set_secondary_markup(self->graphics_row, graphics_hardware_string);
}

static gboolean
use_dark_theme(CcSystemPanel *panel)
{
  AdwStyleManager *style_manager = adw_style_manager_get_default();

  return adw_style_manager_get_dark(style_manager);
}

static void
generic_os_logo(CcSystemPanel *panel)
{
#ifdef DISTRIBUTOR_LOGO
#ifdef DARK_MODE_DISTRIBUTOR_LOGO
  if (use_dark_theme(panel))
  {
    gtk_picture_set_filename(panel->os_logo, DARK_MODE_DISTRIBUTOR_LOGO);
    return;
  }
#endif
  gtk_picture_set_filename(panel->os_logo, DISTRIBUTOR_LOGO);
  return;
#else
  GtkIconTheme *icon_theme;
  g_autofree char *logo_name = g_get_os_info("LOGO");
  g_autoptr(GtkIconPaintable) icon_paintable = NULL;
  g_autoptr(GPtrArray) array = NULL;
  g_autoptr(GIcon) icon = NULL;
  gboolean dark;

  dark = use_dark_theme(panel);
  if (logo_name == NULL)
    logo_name = g_strdup("gnome-logo");

  array = g_ptr_array_new_with_free_func(g_free);
  if (dark)
    g_ptr_array_add(array, (gpointer)g_strdup_printf("%s-text-dark", logo_name));
  g_ptr_array_add(array, (gpointer)g_strdup_printf("%s-text", logo_name));
  if (dark)
    g_ptr_array_add(array, (gpointer)g_strdup_printf("%s-dark", logo_name));
  g_ptr_array_add(array, (gpointer)g_strdup_printf("%s", logo_name));

  icon = g_themed_icon_new_from_names((char **)array->pdata, array->len);
  icon_theme = gtk_icon_theme_get_for_display(gdk_display_get_default());
  icon_paintable = gtk_icon_theme_lookup_by_gicon(icon_theme, icon,
                                                  192,
                                                  gtk_widget_get_scale_factor(GTK_WIDGET(panel)),
                                                  gtk_widget_get_direction(GTK_WIDGET(panel)),
                                                  0);
  gtk_picture_set_paintable(panel->os_logo, GDK_PAINTABLE(icon_paintable));
#endif
}

static void
setup_os_logo(CcSystemPanel *panel)
{

  g_autoptr(GError) oem_error = NULL;

  if (g_key_file_has_key(panel->oem_information, "Logos", "Dark", &oem_error) || use_dark_theme(panel))
  {
    gtk_picture_set_filename(panel->os_logo, g_key_file_get_string(panel->oem_information, "Logos", "Dark", &oem_error));
    if (oem_error != NULL)
    {
      generic_os_logo(panel);
    }
    return;
  }
  else if (g_key_file_has_key(panel->oem_information, "Logos", "Light", &oem_error) || !use_dark_theme(panel))
  {
    gtk_picture_set_filename(panel->os_logo, g_key_file_get_string(panel->oem_information, "Logos", "Light", &oem_error));
    if (oem_error != NULL)
    {
      generic_os_logo(panel);
    }
    return;
  }
  else
  {
    generic_os_logo(panel);
  }
}

static void
cc_system_panel_row_activated_cb(CcSystemPanel *self,
                                 CcListRow *row)
{
  g_assert(CC_IS_SYSTEM_PANEL(self));
  g_assert(CC_IS_LIST_ROW(row));
  g_autoptr(GError) error = NULL;

  if (row == self->homepage_row)
    gtk_show_uri_full(NULL, g_key_file_get_string (self->oem_information, "URLS", "Homepage", &error), GDK_CURRENT_TIME, NULL, NULL, &error);
  else if (row == self->support_row)
    gtk_show_uri_full(NULL, g_key_file_get_string (self->oem_information, "URLS", "Support", &error), GDK_CURRENT_TIME, NULL, NULL, &error);

  if (error != NULL) {
    g_warning ("Error opening URL: %s", error->message);
  }
}

static void
cc_system_panel_class_init(CcSystemPanelClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);

  gtk_widget_class_set_template_from_resource(widget_class, "/org/gnome/control-center/system/cc-system-panel.ui");

  gtk_widget_class_bind_template_child(widget_class, CcSystemPanel, disk_row);
  gtk_widget_class_bind_template_child(widget_class, CcSystemPanel, graphics_row);
  gtk_widget_class_bind_template_child(widget_class, CcSystemPanel, hardware_model_row);
  gtk_widget_class_bind_template_child(widget_class, CcSystemPanel, homepage_row);
  gtk_widget_class_bind_template_child(widget_class, CcSystemPanel, manufacturer_row);
  gtk_widget_class_bind_template_child(widget_class, CcSystemPanel, memory_row);
  gtk_widget_class_bind_template_child(widget_class, CcSystemPanel, os_logo);
  gtk_widget_class_bind_template_child(widget_class, CcSystemPanel, os_type_row);
  gtk_widget_class_bind_template_child(widget_class, CcSystemPanel, processor_row);
  gtk_widget_class_bind_template_child(widget_class, CcSystemPanel, support_row);

  gtk_widget_class_bind_template_callback(widget_class, cc_system_panel_row_activated_cb);

  g_type_ensure(CC_TYPE_LIST_ROW);
}

static void
cc_system_panel_init(CcSystemPanel *self)
{
  gtk_widget_init_template(GTK_WIDGET(self));

  g_resources_register(cc_system_get_resource());

  self->oem_information = g_key_file_new();
  load_oem_information(self);

  if (!g_key_file_has_key(self->oem_information, "URLS", "Homepage", NULL))
  {
    gtk_widget_hide(GTK_WIDGET(self->homepage_row));
  }
  if (!g_key_file_has_key(self->oem_information, "URLS", "Support", NULL))
  {
    gtk_widget_hide(GTK_WIDGET(self->support_row));
  }

  system_panel_setup_overview(self);

  setup_os_logo(self);
}

GtkWidget *
cc_system_panel_new(void)
{
  return g_object_new(CC_TYPE_SYSTEM_PANEL,
                      NULL);
}
