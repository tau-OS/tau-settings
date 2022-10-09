/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * Copyright (C) 2012 Giovanni Campagna <scampa.giovanni@gmail.com>
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

#include <adwaita.h>
#include <string.h>
#include <glib/gi18n-lib.h>
#include <glib.h>
#include <gio/gio.h>
#include <gio/gdesktopappinfo.h>

#include "shell/cc-application.h"
#include "cc-list-row.h"
#include "cc-notifications-panel.h"
#include "cc-notifications-resources.h"
#include "cc-app-notifications-dialog.h"

#define MASTER_SCHEMA "org.gnome.desktop.notifications"
#define APP_SCHEMA MASTER_SCHEMA ".application"
#define APP_PREFIX "/org/gnome/desktop/notifications/application/"
#define NOTIF_MANAGER_SCHEMA "org.gnome.shell.extensions.notification-manager"

struct _CcNotificationsPanel {
  CcPanel            parent_instance;

  GtkButton         *test_btn;
  GtkBox            *horizontal_box;
  GtkToggleButton   *horizontal_left;
  GtkToggleButton   *horizontal_center;
  GtkToggleButton   *horizontal_right;
  GtkBox            *vertical_box;
  GtkToggleButton   *vertical_top;
  GtkToggleButton   *vertical_bottom;

  GtkListBox        *app_listbox;
  CcListRow         *lock_screen_row;
  CcListRow         *dnd_row;
  GtkSizeGroup      *sizegroup1;

  GSettings         *master_settings;
  GSettings         *notif_settings;

  GCancellable      *cancellable;

  GHashTable        *known_applications;

  GDBusProxy        *perm_store;
};

struct _CcNotificationsPanelClass {
  CcPanelClass parent;
};

typedef struct {
  char *canonical_app_id;
  GAppInfo *app_info;
  GSettings *settings;

  /* Temporary pointer, to pass from the loading thread
     to the app */
  CcNotificationsPanel *panel;
} Application;

static void build_app_store (CcNotificationsPanel *panel);
static void select_app      (CcNotificationsPanel *panel, GtkListBoxRow *row);
static int  sort_apps       (gconstpointer one, gconstpointer two, gpointer user_data);

CC_PANEL_REGISTER (CcNotificationsPanel, cc_notifications_panel);

static gboolean
keynav_failed_cb (CcNotificationsPanel *self,
                  GtkDirectionType      direction)
{
  GtkWidget *toplevel = GTK_WIDGET (gtk_widget_get_root (GTK_WIDGET (self)));

  if (!toplevel)
    return FALSE;

  if (direction != GTK_DIR_UP && direction != GTK_DIR_DOWN)
    return FALSE;

  return gtk_widget_child_focus (toplevel, direction == GTK_DIR_UP ?
                                 GTK_DIR_TAB_BACKWARD : GTK_DIR_TAB_FORWARD);
}

static void
cc_notifications_panel_dispose (GObject *object)
{
  CcNotificationsPanel *panel = CC_NOTIFICATIONS_PANEL (object);

  g_clear_object (&panel->master_settings);
  g_clear_pointer (&panel->known_applications, g_hash_table_unref);

  G_OBJECT_CLASS (cc_notifications_panel_parent_class)->dispose (object);
}

static void
cc_notifications_panel_finalize (GObject *object)
{
  CcNotificationsPanel *panel = CC_NOTIFICATIONS_PANEL (object);

  g_clear_object (&panel->perm_store);

  G_OBJECT_CLASS (cc_notifications_panel_parent_class)->finalize (object);
}

static void
on_perm_store_ready (GObject *source_object,
                     GAsyncResult *res,
                     gpointer user_data)
{
  CcNotificationsPanel *self;
  GDBusProxy *proxy;
  g_autoptr(GError) error = NULL;

  proxy = g_dbus_proxy_new_for_bus_finish (res, &error);
  if (proxy == NULL)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
          g_warning ("Failed to connect to xdg-app permission store: %s",
                     error->message);
      return;
    }
  self = user_data;
  self->perm_store = proxy;
}

static void
notif_position_widget_refresh (CcNotificationsPanel *self)
{
  gint horizontal_value = g_settings_get_int (self->notif_settings, "anchor-horizontal");
  gint vertical_value = g_settings_get_int (self->notif_settings, "anchor-vertical");

  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (self->horizontal_left), FALSE);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (self->horizontal_center), FALSE);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (self->horizontal_right), FALSE);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (self->vertical_top), FALSE);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (self->vertical_bottom), FALSE);

  // Handle Horizontal Values
  if (horizontal_value == 0)
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (self->horizontal_left), TRUE);
  if (horizontal_value == 1)
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (self->horizontal_right), TRUE);
  if (horizontal_value == 2)
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (self->horizontal_center), TRUE);
  // Handle Vertical Values
  if (vertical_value == 0)
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (self->vertical_top), TRUE);
  if (vertical_value == 1)
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (self->vertical_bottom), TRUE);

}

static void
on_pos_left_toggled (CcNotificationsPanel *panel)
{
  if (g_settings_get_int (panel->notif_settings, "anchor-horizontal") != 0)
    g_settings_set_int (panel->notif_settings, "anchor-horizontal", 0);
}
static void
on_pos_center_toggled (CcNotificationsPanel *panel)
{
  if (g_settings_get_int (panel->notif_settings, "anchor-horizontal") != 2)
    g_settings_set_int (panel->notif_settings, "anchor-horizontal", 2);
}
static void
on_pos_right_toggled (CcNotificationsPanel *panel)
{
  if (g_settings_get_int (panel->notif_settings, "anchor-horizontal") != 1)
    g_settings_set_int (panel->notif_settings, "anchor-horizontal", 1);
}
static void
on_pos_top_toggled (CcNotificationsPanel *panel)
{
  if (g_settings_get_int (panel->notif_settings, "anchor-vertical") != 0)
    g_settings_set_int (panel->notif_settings, "anchor-vertical", 0);
}
static void
on_pos_bottom_toggled (CcNotificationsPanel *panel)
{
  if (g_settings_get_int (panel->notif_settings, "anchor-vertical") != 1)
    g_settings_set_int (panel->notif_settings, "anchor-vertical", 1);
}
static void
on_test_btn_clicked (CcNotificationsPanel *panel)
{
  GApplication *application;
  application = G_APPLICATION (g_application_get_default ());

  GNotification *notification = g_notification_new ("Hello World!");
  g_application_send_notification (application, "settings-test-notification", notification);
}

static void
cc_notifications_panel_init (CcNotificationsPanel *panel)
{
  g_autoptr(GSettingsSchema) notif_schema = NULL;
  g_resources_register (cc_notifications_get_resource ());

  gtk_widget_init_template (GTK_WIDGET (panel));

  panel->known_applications = g_hash_table_new_full (g_str_hash, g_str_equal,
                                                     NULL, g_free);

  panel->master_settings = g_settings_new (MASTER_SCHEMA);

  /* Only load if we have Notification Manager installed */
  notif_schema = g_settings_schema_source_lookup (g_settings_schema_source_get_default (),
                                            NOTIF_MANAGER_SCHEMA, 
                                            TRUE);

  if (!notif_schema) {
    g_warning ("No notification manager is installed here. Please fix your installation.");
    return;
  }

  panel->notif_settings = g_settings_new_full (notif_schema, NULL, NULL);

  g_settings_bind (panel->master_settings, "show-banners",
                   panel->dnd_row,
                   "active", G_SETTINGS_BIND_INVERT_BOOLEAN);
  g_settings_bind (panel->master_settings, "show-in-lock-screen",
                   panel->lock_screen_row,
                   "active", G_SETTINGS_BIND_DEFAULT);
  
  g_signal_connect_object (panel->notif_settings,
                           "changed::anchor-horizontal",
                           G_CALLBACK (notif_position_widget_refresh),
                           panel,
                           G_CONNECT_SWAPPED);
  g_signal_connect_object (panel->notif_settings,
                           "changed::anchor-vertical",
                           G_CALLBACK (notif_position_widget_refresh),
                           panel,
                           G_CONNECT_SWAPPED);
  notif_position_widget_refresh (panel);

  gtk_list_box_set_sort_func (panel->app_listbox, (GtkListBoxSortFunc)sort_apps, NULL, NULL);

  build_app_store (panel);

  g_dbus_proxy_new_for_bus (G_BUS_TYPE_SESSION,
                            G_DBUS_PROXY_FLAGS_NONE,
                            NULL,
                            "org.freedesktop.impl.portal.PermissionStore",
                            "/org/freedesktop/impl/portal/PermissionStore",
                            "org.freedesktop.impl.portal.PermissionStore",
                            cc_panel_get_cancellable (CC_PANEL (panel)),
                            on_perm_store_ready,
                            panel);
}

static const char *
cc_notifications_panel_get_help_uri (CcPanel *panel)
{
  return "help:gnome-help/shell-notifications";
}

static void
cc_notifications_panel_class_init (CcNotificationsPanelClass *klass)
{
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  CcPanelClass   *panel_class  = CC_PANEL_CLASS (klass);

  panel_class->get_help_uri = cc_notifications_panel_get_help_uri;

  /* Separate dispose() and finalize() functions are necessary
   * to make sure we cancel the running thread before the panel
   * gets finalized */
  object_class->dispose = cc_notifications_panel_dispose;
  object_class->finalize = cc_notifications_panel_finalize;

  g_type_ensure (CC_TYPE_LIST_ROW);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/notifications/cc-notifications-panel.ui");

  gtk_widget_class_bind_template_child (widget_class, CcNotificationsPanel, app_listbox);
  gtk_widget_class_bind_template_child (widget_class, CcNotificationsPanel, lock_screen_row);
  gtk_widget_class_bind_template_child (widget_class, CcNotificationsPanel, dnd_row);
  gtk_widget_class_bind_template_child (widget_class, CcNotificationsPanel, sizegroup1);
  gtk_widget_class_bind_template_child (widget_class, CcNotificationsPanel, test_btn);
  gtk_widget_class_bind_template_child (widget_class, CcNotificationsPanel, horizontal_left);
  gtk_widget_class_bind_template_child (widget_class, CcNotificationsPanel, horizontal_center);
  gtk_widget_class_bind_template_child (widget_class, CcNotificationsPanel, horizontal_right);
  gtk_widget_class_bind_template_child (widget_class, CcNotificationsPanel, vertical_top);
  gtk_widget_class_bind_template_child (widget_class, CcNotificationsPanel, vertical_bottom);

  gtk_widget_class_bind_template_callback (widget_class, select_app);
  gtk_widget_class_bind_template_callback (widget_class, keynav_failed_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_pos_left_toggled);
  gtk_widget_class_bind_template_callback (widget_class, on_pos_center_toggled);
  gtk_widget_class_bind_template_callback (widget_class, on_pos_right_toggled);
  gtk_widget_class_bind_template_callback (widget_class, on_pos_top_toggled);
  gtk_widget_class_bind_template_callback (widget_class, on_pos_bottom_toggled);
  gtk_widget_class_bind_template_callback (widget_class, on_test_btn_clicked);
}

static inline GQuark
application_quark (void)
{
  static GQuark quark;

  if (G_UNLIKELY (quark == 0))
    quark = g_quark_from_static_string ("cc-application");

  return quark;
}

static gboolean
on_off_label_mapping_get (GValue   *value,
                          GVariant *variant,
                          gpointer  user_data)
{
  g_value_set_string (value, g_variant_get_boolean (variant) ? _("On") : _("Off"));

  return TRUE;
}

static void
application_free (Application *app)
{
  g_free (app->canonical_app_id);
  g_object_unref (app->app_info);
  g_object_unref (app->settings);

  g_slice_free (Application, app);
}

G_DEFINE_AUTOPTR_CLEANUP_FUNC (Application, application_free)

static void
add_application (CcNotificationsPanel *panel,
                 Application          *app)
{
  GtkWidget *w, *row;
  g_autoptr(GIcon) icon = NULL;
  const gchar *app_name;

  app_name = g_app_info_get_name (app->app_info);
  if (app_name == NULL || *app_name == '\0')
    return;

  icon = g_app_info_get_icon (app->app_info);
  if (icon == NULL)
    icon = g_themed_icon_new ("application-x-executable");
  else
    g_object_ref (icon);

  row = adw_action_row_new ();
  gtk_list_box_row_set_activatable (GTK_LIST_BOX_ROW (row), TRUE);
  adw_preferences_row_set_title (ADW_PREFERENCES_ROW (row),
                                 g_markup_escape_text (app_name, -1));

  g_object_set_qdata_full (G_OBJECT (row), application_quark (),
                           app, (GDestroyNotify) application_free);

  gtk_list_box_append (panel->app_listbox, row);

  w = gtk_image_new_from_gicon (icon);
  gtk_style_context_add_class (gtk_widget_get_style_context (w), "lowres-icon");
  gtk_image_set_icon_size (GTK_IMAGE (w), GTK_ICON_SIZE_LARGE);
  adw_action_row_add_prefix (ADW_ACTION_ROW (row), w);

  w = gtk_label_new ("");
  g_settings_bind_with_mapping (app->settings, "enable",
                                w, "label",
                                G_SETTINGS_BIND_GET |
                                G_SETTINGS_BIND_NO_SENSITIVITY,
                                on_off_label_mapping_get,
                                NULL,
                                NULL,
                                NULL);
  adw_action_row_add_suffix (ADW_ACTION_ROW (row), w);

  w = gtk_image_new_from_icon_name ("go-next-symbolic");
  adw_action_row_add_suffix (ADW_ACTION_ROW (row), w);

  g_hash_table_add (panel->known_applications, g_strdup (app->canonical_app_id));
}

static void
maybe_add_app_id (CcNotificationsPanel *panel,
                  const char *canonical_app_id)
{
  Application *app;
  g_autofree gchar *path = NULL;
  g_autofree gchar *full_app_id = NULL;
  g_autoptr(GSettings) settings = NULL;
  g_autoptr(GAppInfo) app_info = NULL;

  if (*canonical_app_id == '\0')
    return;

  if (g_hash_table_contains (panel->known_applications,
                             canonical_app_id))
    return;

  path = g_strconcat (APP_PREFIX, canonical_app_id, "/", NULL);
  settings = g_settings_new_with_path (APP_SCHEMA, path);

  full_app_id = g_settings_get_string (settings, "application-id");
  app_info = G_APP_INFO (g_desktop_app_info_new (full_app_id));

  if (app_info == NULL) {
    g_debug ("Not adding application '%s' (canonical app ID: %s)",
             full_app_id, canonical_app_id);
    /* The application cannot be found, probably it was uninstalled */
    return;
  }

  app = g_slice_new (Application);
  app->canonical_app_id = g_strdup (canonical_app_id);
  app->settings = g_object_ref (settings);
  app->app_info = g_object_ref (app_info);

  g_debug ("Adding application '%s' (canonical app ID: %s)",
           full_app_id, canonical_app_id);

  add_application (panel, app);
}

static char *
app_info_get_id (GAppInfo *app_info)
{
  const char *desktop_id;
  g_autofree gchar *ret = NULL;
  const char *filename;
  int l;

  desktop_id = g_app_info_get_id (app_info);
  if (desktop_id != NULL)
    {
      ret = g_strdup (desktop_id);
    }
  else
    {
      filename = g_desktop_app_info_get_filename (G_DESKTOP_APP_INFO (app_info));
      ret = g_path_get_basename (filename);
    }

  if (G_UNLIKELY (g_str_has_suffix (ret, ".desktop") == FALSE))
    return NULL;

  l = strlen (ret);
  *(ret + l - strlen(".desktop")) = '\0';
  return g_steal_pointer (&ret);
}

static void
process_app_info (CcNotificationsPanel *panel,
                  GAppInfo             *app_info)
{
  Application *app;
  g_autofree gchar *app_id = NULL;
  g_autofree gchar *path = NULL;
  g_autoptr(GSettings) settings = NULL;
  guint i;

  app_id = app_info_get_id (app_info);
  g_strcanon (app_id,
              "0123456789"
              "abcdefghijklmnopqrstuvwxyz"
              "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
              "-",
              '-');
  for (i = 0; app_id[i] != '\0'; i++)
    app_id[i] = g_ascii_tolower (app_id[i]);

  path = g_strconcat (APP_PREFIX, app_id, "/", NULL);
  settings = g_settings_new_with_path (APP_SCHEMA, path);

  app = g_slice_new (Application);
  app->canonical_app_id = g_steal_pointer (&app_id);
  app->settings = g_object_ref (settings);
  app->app_info = g_object_ref (app_info);
  app->panel = g_object_ref (panel);

  if (g_hash_table_contains (panel->known_applications,
                             app->canonical_app_id))
    return;

  g_debug ("Processing queued application %s", app->canonical_app_id);

  add_application (panel, app);
}

static void
load_apps (CcNotificationsPanel *panel)
{
  GList *iter, *apps;

  apps = g_app_info_get_all ();

  for (iter = apps; iter; iter = iter->next)
    {
      GDesktopAppInfo *app;

      app = iter->data;
      if (g_desktop_app_info_get_boolean (app, "X-GNOME-UsesNotifications")) {
        process_app_info (panel, G_APP_INFO (app));
        g_debug ("Processing app '%s'", g_app_info_get_id (G_APP_INFO (app)));
      } else {
        g_debug ("Skipped app '%s', doesn't use notifications", g_app_info_get_id (G_APP_INFO (app)));
      }
    }

  g_list_free_full (apps, g_object_unref);
}

static void
children_changed (CcNotificationsPanel *panel,
                  const char           *key)
{
  int i;
  g_auto (GStrv) new_app_ids = NULL;

  g_settings_get (panel->master_settings,
                  "application-children",
                  "^as", &new_app_ids);
  for (i = 0; new_app_ids[i]; i++)
    maybe_add_app_id (panel, new_app_ids[i]);
}

static void
build_app_store (CcNotificationsPanel *panel)
{
  /* Build application entries for known applications */
  children_changed (panel, NULL);
  g_signal_connect_object (panel->master_settings,
                           "changed::application-children",
                           G_CALLBACK (children_changed), panel, G_CONNECT_SWAPPED);

  /* Scan applications that statically declare to show notifications */
  load_apps (panel);
}

static void
select_app (CcNotificationsPanel *panel,
            GtkListBoxRow        *row)
{
  Application *app;
  g_autofree gchar *app_id = NULL;
  CcAppNotificationsDialog *dialog;
  GtkWidget *toplevel;
  CcShell *shell;

  shell = cc_panel_get_shell (CC_PANEL (panel));
  toplevel = cc_shell_get_toplevel (shell);

  app = g_object_get_qdata (G_OBJECT (row), application_quark ());

  app_id = g_strdup (g_app_info_get_id (app->app_info));
  if (g_str_has_suffix (app_id, ".desktop"))
    app_id[strlen (app_id) - strlen (".desktop")] = '\0';

  dialog = cc_app_notifications_dialog_new (app_id, g_app_info_get_name (app->app_info), app->settings, panel->master_settings, panel->perm_store);
  gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (toplevel));
  gtk_widget_show (GTK_WIDGET (dialog));
}

static int
sort_apps (gconstpointer one,
           gconstpointer two,
           gpointer      user_data)
{
  Application *a1, *a2;

  a1 = g_object_get_qdata (G_OBJECT (one), application_quark ());
  a2 = g_object_get_qdata (G_OBJECT (two), application_quark ());

  return g_utf8_collate (g_app_info_get_name (a1->app_info),
                         g_app_info_get_name (a2->app_info));
}
