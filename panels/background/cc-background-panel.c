/*
 * Copyright (C) 2010 Intel, Inc
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
 * Author: Thomas Wood <thomas.wood@intel.com>
 *
 */

#include <config.h>
#include <stdio.h>

#include <string.h>
#include <glib.h>
#include <glib/gi18n-lib.h>
#include <glib/gstdio.h>
#include <gdk/gdk.h>

#include <gdesktop-enums.h>

#include "cc-background-panel.h"

#include "cc-background-chooser.h"
#include "cc-background-item.h"
#include "cc-background-preview.h"
#include "cc-background-resources.h"
#include "cc-background-xml.h"

#define WP_PATH_ID "org.gnome.desktop.background"
#define WP_LOCK_PATH_ID "org.gnome.desktop.screensaver"
#define WP_URI_KEY "picture-uri"
#define WP_URI_DARK_KEY "picture-uri-dark"
#define WP_OPTIONS_KEY "picture-options"
#define WP_SHADING_KEY "color-shading-type"
#define WP_PCOLOR_KEY "primary-color"
#define WP_SCOLOR_KEY "secondary-color"

#define INTERFACE_PATH_ID "org.gnome.desktop.interface"
#define INTERFACE_COLOR_SCHEME_KEY "color-scheme"
#define INTERFACE_GTK_THEME_KEY "gtk-theme"
#define INTERFACE_ACCENT_COLOR_KEY "accent-color"
#define SHELL_PATH_ID "org.gnome.shell.extensions.user-theme"
#define INTERFACE_SHELL_THEME_KEY "name"

// Constants for each accent color
const double redd[] = {0.8588, 0.1568, 0.3764};
GVariant* redarr;

const double oranged[] = {0.9686, 0.5058, 0.168};
GVariant* orangearr;

const double yellowd[] = {0.8784, 0.6313, 0.0039};
GVariant* yellowarr;

const double greend[] = {0.2862, 0.8156, 0.3686};
GVariant* greenarr;

const double mintd[] = {0.3372, 0.7490, 0.6509};
GVariant* mintarr;

const double blued[] = {0.1490, 0.5568, 0.9764};
GVariant* bluearr;

const double purpled[] = {0.5490, 0.3372, 0.7490};
GVariant* purplearr;

const double pinkd[] = {0.7490, 0.3372, 0.6588};
GVariant* pinkarr;

struct _CcBackgroundPanel
{
  CcPanel parent_instance;

  GDBusConnection *connection;

  GSettings *settings;
  GSettings *lock_settings;
  GSettings *interface_settings;
  GSettings *theme_settings;

  GnomeDesktopThumbnailFactory *thumb_factory;
  GDBusProxy *proxy;

  CcBackgroundItem *current_background;

  CcBackgroundChooser *background_chooser;
  CcBackgroundPreview *default_preview;
  CcBackgroundPreview *dark_preview;
  GtkToggleButton *default_toggle;
  GtkToggleButton *dark_toggle;
  
  // Accent Buttons
  GtkBox         *color_box;
  GtkCheckButton *red;
  GtkCheckButton *orange;
  GtkCheckButton *yellow;
  GtkCheckButton *green;
  GtkCheckButton *blue;
  GtkCheckButton *purple;
  GtkCheckButton *pink;
  GtkCheckButton *mint;
};

CC_PANEL_REGISTER (CcBackgroundPanel, cc_background_panel)

static void
load_custom_css (CcBackgroundPanel *self)
{
  g_autoptr(GtkCssProvider) provider = NULL;

  provider = gtk_css_provider_new ();
  gtk_css_provider_load_from_resource (provider, "/org/gnome/control-center/background/preview.css");
  gtk_style_context_add_provider_for_display (gdk_display_get_default (),
                                              GTK_STYLE_PROVIDER (provider),
                                              GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
}

static void
reload_color_scheme_toggles (CcBackgroundPanel *self)
{
  GDesktopColorScheme scheme;

  scheme = g_settings_get_enum (self->interface_settings, INTERFACE_COLOR_SCHEME_KEY);

  if (scheme == G_DESKTOP_COLOR_SCHEME_DEFAULT)
    {
      gtk_toggle_button_set_active (self->default_toggle, TRUE);
    }
  else if (scheme == G_DESKTOP_COLOR_SCHEME_PREFER_DARK)
    {
      gtk_toggle_button_set_active (self->dark_toggle, TRUE);
    }
  else
    {
      gtk_toggle_button_set_active (self->default_toggle, FALSE);
      gtk_toggle_button_set_active (self->dark_toggle, FALSE);
    }
}

static void
transition_screen (CcBackgroundPanel *self)
{
  g_autoptr (GError) error = NULL;

  if (!self->proxy)
    return;

  g_dbus_proxy_call_sync (self->proxy,
                          "ScreenTransition",
                          NULL,
                          G_DBUS_CALL_FLAGS_NONE,
                          -1,
                          NULL,
                          &error);

  if (error)
    g_warning ("Couldn't transition screen: %s", error->message);
}

static void
set_color_scheme (CcBackgroundPanel   *self,
                  GDesktopColorScheme  color_scheme)
{
  GDesktopColorScheme scheme;

  scheme = g_settings_get_enum (self->interface_settings,
                                INTERFACE_COLOR_SCHEME_KEY);

  /* We have to check the equality manually to avoid starting an unnecessary
   * screen transition */
  if (color_scheme == scheme)
    return;

  transition_screen (self);

  g_settings_set_enum (self->interface_settings,
                       INTERFACE_COLOR_SCHEME_KEY,
                       color_scheme);
}

/* Color schemes */

static void
on_color_scheme_toggle_active_cb (CcBackgroundPanel *self)
{
  if (gtk_toggle_button_get_active (self->default_toggle))
    {
      set_color_scheme (self, G_DESKTOP_COLOR_SCHEME_DEFAULT);
      g_settings_set_string (self->interface_settings,
                             INTERFACE_GTK_THEME_KEY,
                             "Helium");
      g_settings_set_string (self->theme_settings,
                             INTERFACE_SHELL_THEME_KEY,
                             "Helium");
    }
  else if (gtk_toggle_button_get_active (self->dark_toggle))
    {
      set_color_scheme (self, G_DESKTOP_COLOR_SCHEME_PREFER_DARK);
      g_settings_set_string (self->interface_settings,
                             INTERFACE_GTK_THEME_KEY,
                             "Helium-dark");
      g_settings_set_string (self->theme_settings,
                             INTERFACE_SHELL_THEME_KEY,
                             "Helium-dark");
    }
}

static void
got_transition_proxy_cb (GObject      *source_object,
                         GAsyncResult *res,
                         gpointer      data)
{
  g_autoptr(GError) error = NULL;
  CcBackgroundPanel *self = data;

  self->proxy = g_dbus_proxy_new_for_bus_finish (res, &error);

  if (self->proxy == NULL)
    {
      g_warning ("Error creating proxy: %s", error->message);
      return;
    }
}

/* Background */

static void
update_preview (CcBackgroundPanel *panel)
{
  CcBackgroundItem *current_background;

  current_background = panel->current_background;
  cc_background_preview_set_item (panel->default_preview, current_background);
  cc_background_preview_set_item (panel->dark_preview, current_background);
}

static gchar *
get_save_path (void)
{
  return g_build_filename (g_get_user_config_dir (),
                           "gnome-control-center",
                           "backgrounds",
                           "last-edited.xml",
                           NULL);
}

static void
reload_current_bg (CcBackgroundPanel *panel)
{
  g_autoptr(CcBackgroundItem) saved = NULL;
  CcBackgroundItem *configured;
  GSettings *settings = NULL;
  g_autofree gchar *uri = NULL;
  g_autofree gchar *dark_uri = NULL;
  g_autofree gchar *pcolor = NULL;
  g_autofree gchar *scolor = NULL;

  /* Load the saved configuration */
  uri = get_save_path ();
  saved = cc_background_xml_get_item (uri);

  /* initalise the current background information from settings */
  settings = panel->settings;
  uri = g_settings_get_string (settings, WP_URI_KEY);
  if (uri && *uri == '\0')
    g_clear_pointer (&uri, g_free);


  configured = cc_background_item_new (uri);

  dark_uri = g_settings_get_string (settings, WP_URI_DARK_KEY);
  pcolor = g_settings_get_string (settings, WP_PCOLOR_KEY);
  scolor = g_settings_get_string (settings, WP_SCOLOR_KEY);
  g_object_set (G_OBJECT (configured),
                "name", _("Current background"),
                "uri-dark", dark_uri,
                "placement", g_settings_get_enum (settings, WP_OPTIONS_KEY),
                "shading", g_settings_get_enum (settings, WP_SHADING_KEY),
                "primary-color", pcolor,
                "secondary-color", scolor,
                NULL);

  if (saved != NULL && cc_background_item_compare (saved, configured))
    {
      CcBackgroundItemFlags flags;
      flags = cc_background_item_get_flags (saved);
      /* Special case for colours */
      if (cc_background_item_get_placement (saved) == G_DESKTOP_BACKGROUND_STYLE_NONE)
        flags &=~ (CC_BACKGROUND_ITEM_HAS_PCOLOR | CC_BACKGROUND_ITEM_HAS_SCOLOR);
      g_object_set (G_OBJECT (configured),
		    "name", cc_background_item_get_name (saved),
		    "flags", flags,
		    "source-url", cc_background_item_get_source_url (saved),
		    "source-xml", cc_background_item_get_source_xml (saved),
		    NULL);
    }

  g_clear_object (&panel->current_background);
  panel->current_background = configured;
  cc_background_item_load (configured, NULL);
}

static gboolean
create_save_dir (void)
{
  g_autofree char *path = NULL;

  path = g_build_filename (g_get_user_config_dir (),
			   "gnome-control-center",
			   "backgrounds",
			   NULL);
  if (g_mkdir_with_parents (path, USER_DIR_MODE) < 0)
    {
      g_warning ("Failed to create directory '%s'", path);
      return FALSE;
    }
  return TRUE;
}

static void
set_background (CcBackgroundPanel *panel,
                GSettings         *settings,
                CcBackgroundItem  *item,
                gboolean           set_dark)
{
  GDesktopBackgroundStyle style;
  CcBackgroundItemFlags flags;
  g_autofree gchar *filename = NULL;
  const char *uri;

  if (item == NULL)
    return;

  uri = cc_background_item_get_uri (item);
  flags = cc_background_item_get_flags (item);

  g_settings_set_string (settings, WP_URI_KEY, uri);

  if (set_dark)
    {
      const char *uri_dark;

      uri_dark = cc_background_item_get_uri_dark (item);

      if (uri_dark && uri_dark[0])
        g_settings_set_string (settings, WP_URI_DARK_KEY, uri_dark);
      else
        g_settings_set_string (settings, WP_URI_DARK_KEY, uri);
    }

  /* Also set the placement if we have a URI and the previous value was none */
  if (flags & CC_BACKGROUND_ITEM_HAS_PLACEMENT)
    {
      g_settings_set_enum (settings, WP_OPTIONS_KEY, cc_background_item_get_placement (item));
    }
  else if (uri != NULL)
    {
      style = g_settings_get_enum (settings, WP_OPTIONS_KEY);
      if (style == G_DESKTOP_BACKGROUND_STYLE_NONE)
        g_settings_set_enum (settings, WP_OPTIONS_KEY, cc_background_item_get_placement (item));
    }

  if (flags & CC_BACKGROUND_ITEM_HAS_SHADING)
    g_settings_set_enum (settings, WP_SHADING_KEY, cc_background_item_get_shading (item));

  g_settings_set_string (settings, WP_PCOLOR_KEY, cc_background_item_get_pcolor (item));
  g_settings_set_string (settings, WP_SCOLOR_KEY, cc_background_item_get_scolor (item));

  /* Apply all changes */
  g_settings_apply (settings);

  /* Save the source XML if there is one */
  filename = get_save_path ();
  if (create_save_dir ())
    cc_background_xml_save (panel->current_background, filename);
}

static void
on_chooser_background_chosen_cb (CcBackgroundPanel *self,
                                 CcBackgroundItem  *item)
{
  set_background (self, self->settings, item, TRUE);
  set_background (self, self->lock_settings, item, FALSE);
}

static void
on_add_picture_button_clicked_cb (CcBackgroundPanel *self)
{
  cc_background_chooser_select_file (self->background_chooser);
}

static void
on_red_toggled (CcBackgroundPanel *self)
{
  if (g_settings_get_value (self->interface_settings, INTERFACE_ACCENT_COLOR_KEY) != redarr)
    g_settings_set_value (self->interface_settings, INTERFACE_ACCENT_COLOR_KEY, redarr);
}
static void
on_orange_toggled (CcBackgroundPanel *self)
{
  if (g_settings_get_value (self->interface_settings, INTERFACE_ACCENT_COLOR_KEY) != orangearr)
    g_settings_set_value (self->interface_settings, INTERFACE_ACCENT_COLOR_KEY, orangearr);
}
static void
on_yellow_toggled (CcBackgroundPanel *self)
{
  if (g_settings_get_value (self->interface_settings, INTERFACE_ACCENT_COLOR_KEY) != yellowarr)
    g_settings_set_value (self->interface_settings, INTERFACE_ACCENT_COLOR_KEY, yellowarr);
}
static void
on_green_toggled (CcBackgroundPanel *self)
{
  if (g_settings_get_value (self->interface_settings, INTERFACE_ACCENT_COLOR_KEY) != greenarr)
    g_settings_set_value (self->interface_settings, INTERFACE_ACCENT_COLOR_KEY, greenarr);
}
static void
on_mint_toggled (CcBackgroundPanel *self)
{
  if (g_settings_get_value (self->interface_settings, INTERFACE_ACCENT_COLOR_KEY) != mintarr)
    g_settings_set_value (self->interface_settings, INTERFACE_ACCENT_COLOR_KEY, mintarr);
}
static void
on_blue_toggled (CcBackgroundPanel *self)
{
  if (g_settings_get_value (self->interface_settings, INTERFACE_ACCENT_COLOR_KEY) != bluearr)
    g_settings_set_value (self->interface_settings, INTERFACE_ACCENT_COLOR_KEY, bluearr);
}
static void
on_purple_toggled (CcBackgroundPanel *self)
{
  if (g_settings_get_value (self->interface_settings, INTERFACE_ACCENT_COLOR_KEY) != purplearr)
    g_settings_set_value (self->interface_settings, INTERFACE_ACCENT_COLOR_KEY, purplearr);
}
static void
on_pink_toggled (CcBackgroundPanel *self)
{
  if (g_settings_get_value (self->interface_settings, INTERFACE_ACCENT_COLOR_KEY) != pinkarr)
    g_settings_set_value (self->interface_settings, INTERFACE_ACCENT_COLOR_KEY, pinkarr);
}
static void
accent_refresh (CcBackgroundPanel *self)
{
  GVariant double[] value = g_settings_get_value (self->interface_settings, INTERFACE_ACCENT_COLOR_KEY);

  if (value == redarr) {
    gtk_check_button_set_active (GTK_CHECK_BUTTON (self->red), TRUE);
    gtk_check_button_set_active (GTK_CHECK_BUTTON (self->orange), FALSE);
    gtk_check_button_set_active (GTK_CHECK_BUTTON (self->yellow), FALSE);
    gtk_check_button_set_active (GTK_CHECK_BUTTON (self->green), FALSE);
    gtk_check_button_set_active (GTK_CHECK_BUTTON (self->mint), FALSE);
    gtk_check_button_set_active (GTK_CHECK_BUTTON (self->blue), FALSE);
    gtk_check_button_set_active (GTK_CHECK_BUTTON (self->purple), FALSE);
    gtk_check_button_set_active (GTK_CHECK_BUTTON (self->pink), FALSE);
  } else if (value == orangearr) {
    gtk_check_button_set_active (GTK_CHECK_BUTTON (self->red), FALSE);
    gtk_check_button_set_active (GTK_CHECK_BUTTON (self->orange), TRUE);
    gtk_check_button_set_active (GTK_CHECK_BUTTON (self->yellow), FALSE);
    gtk_check_button_set_active (GTK_CHECK_BUTTON (self->green), FALSE);
    gtk_check_button_set_active (GTK_CHECK_BUTTON (self->mint), FALSE);
    gtk_check_button_set_active (GTK_CHECK_BUTTON (self->blue), FALSE);
    gtk_check_button_set_active (GTK_CHECK_BUTTON (self->purple), FALSE);
    gtk_check_button_set_active (GTK_CHECK_BUTTON (self->pink), FALSE);
  } else if (value == yellowarr) {
    gtk_check_button_set_active (GTK_CHECK_BUTTON (self->red), FALSE);
    gtk_check_button_set_active (GTK_CHECK_BUTTON (self->orange), FALSE);
    gtk_check_button_set_active (GTK_CHECK_BUTTON (self->yellow), TRUE);
    gtk_check_button_set_active (GTK_CHECK_BUTTON (self->green), FALSE);
    gtk_check_button_set_active (GTK_CHECK_BUTTON (self->mint), FALSE);
    gtk_check_button_set_active (GTK_CHECK_BUTTON (self->blue), FALSE);
    gtk_check_button_set_active (GTK_CHECK_BUTTON (self->purple), FALSE);
    gtk_check_button_set_active (GTK_CHECK_BUTTON (self->pink), FALSE);
  } else if (value == greenarr) {
    gtk_check_button_set_active (GTK_CHECK_BUTTON (self->red), FALSE);
    gtk_check_button_set_active (GTK_CHECK_BUTTON (self->orange), FALSE);
    gtk_check_button_set_active (GTK_CHECK_BUTTON (self->yellow), FALSE);
    gtk_check_button_set_active (GTK_CHECK_BUTTON (self->green), TRUE);
    gtk_check_button_set_active (GTK_CHECK_BUTTON (self->mint), FALSE);
    gtk_check_button_set_active (GTK_CHECK_BUTTON (self->blue), FALSE);
    gtk_check_button_set_active (GTK_CHECK_BUTTON (self->purple), FALSE);
    gtk_check_button_set_active (GTK_CHECK_BUTTON (self->pink), FALSE);
  } else if (value == mintarr) {
    gtk_check_button_set_active (GTK_CHECK_BUTTON (self->red), FALSE);
    gtk_check_button_set_active (GTK_CHECK_BUTTON (self->orange), FALSE);
    gtk_check_button_set_active (GTK_CHECK_BUTTON (self->yellow), FALSE);
    gtk_check_button_set_active (GTK_CHECK_BUTTON (self->green), FALSE);
    gtk_check_button_set_active (GTK_CHECK_BUTTON (self->mint), TRUE);
    gtk_check_button_set_active (GTK_CHECK_BUTTON (self->blue), FALSE);
    gtk_check_button_set_active (GTK_CHECK_BUTTON (self->purple), FALSE);
    gtk_check_button_set_active (GTK_CHECK_BUTTON (self->pink), FALSE);
  } else if (value == bluearr) {
    gtk_check_button_set_active (GTK_CHECK_BUTTON (self->red), FALSE);
    gtk_check_button_set_active (GTK_CHECK_BUTTON (self->orange), FALSE);
    gtk_check_button_set_active (GTK_CHECK_BUTTON (self->yellow), FALSE);
    gtk_check_button_set_active (GTK_CHECK_BUTTON (self->green), FALSE);
    gtk_check_button_set_active (GTK_CHECK_BUTTON (self->mint), FALSE);
    gtk_check_button_set_active (GTK_CHECK_BUTTON (self->blue), TRUE);
    gtk_check_button_set_active (GTK_CHECK_BUTTON (self->purple), FALSE);
    gtk_check_button_set_active (GTK_CHECK_BUTTON (self->pink), FALSE);
  } else if (value == purplearr) {
    gtk_check_button_set_active (GTK_CHECK_BUTTON (self->red), FALSE);
    gtk_check_button_set_active (GTK_CHECK_BUTTON (self->orange), FALSE);
    gtk_check_button_set_active (GTK_CHECK_BUTTON (self->yellow), FALSE);
    gtk_check_button_set_active (GTK_CHECK_BUTTON (self->green), FALSE);
    gtk_check_button_set_active (GTK_CHECK_BUTTON (self->mint), FALSE);
    gtk_check_button_set_active (GTK_CHECK_BUTTON (self->blue), FALSE);
    gtk_check_button_set_active (GTK_CHECK_BUTTON (self->purple), TRUE);
    gtk_check_button_set_active (GTK_CHECK_BUTTON (self->pink), FALSE);
  } else if (value == pinkarr) {
    gtk_check_button_set_active (GTK_CHECK_BUTTON (self->red), FALSE);
    gtk_check_button_set_active (GTK_CHECK_BUTTON (self->orange), FALSE);
    gtk_check_button_set_active (GTK_CHECK_BUTTON (self->yellow), FALSE);
    gtk_check_button_set_active (GTK_CHECK_BUTTON (self->green), FALSE);
    gtk_check_button_set_active (GTK_CHECK_BUTTON (self->mint), FALSE);
    gtk_check_button_set_active (GTK_CHECK_BUTTON (self->blue), FALSE);
    gtk_check_button_set_active (GTK_CHECK_BUTTON (self->purple), FALSE);
    gtk_check_button_set_active (GTK_CHECK_BUTTON (self->pink), TRUE);
  } else {
    gtk_check_button_set_active (GTK_CHECK_BUTTON (self->red), FALSE);
    gtk_check_button_set_active (GTK_CHECK_BUTTON (self->orange), FALSE);
    gtk_check_button_set_active (GTK_CHECK_BUTTON (self->yellow), FALSE);
    gtk_check_button_set_active (GTK_CHECK_BUTTON (self->green), FALSE);
    gtk_check_button_set_active (GTK_CHECK_BUTTON (self->mint), FALSE);
    gtk_check_button_set_active (GTK_CHECK_BUTTON (self->blue), FALSE);
    gtk_check_button_set_active (GTK_CHECK_BUTTON (self->purple), TRUE);
    gtk_check_button_set_active (GTK_CHECK_BUTTON (self->pink), FALSE);
  }
}

static const char *
cc_background_panel_get_help_uri (CcPanel *panel)
{
  return "help:gnome-help/look-background";
}

static void
cc_background_panel_dispose (GObject *object)
{
  CcBackgroundPanel *panel = CC_BACKGROUND_PANEL (object);

  g_clear_object (&panel->settings);
  g_clear_object (&panel->lock_settings);
  g_clear_object (&panel->interface_settings);
  g_clear_object (&panel->theme_settings);
  g_clear_object (&panel->thumb_factory);
  g_clear_object (&panel->proxy);

  G_OBJECT_CLASS (cc_background_panel_parent_class)->dispose (object);
}

static void
cc_background_panel_finalize (GObject *object)
{
  CcBackgroundPanel *panel = CC_BACKGROUND_PANEL (object);

  g_clear_object (&panel->current_background);

  G_OBJECT_CLASS (cc_background_panel_parent_class)->finalize (object);
}

static void
cc_background_panel_class_init (CcBackgroundPanelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  CcPanelClass *panel_class = CC_PANEL_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  g_type_ensure (CC_TYPE_BACKGROUND_CHOOSER);
  g_type_ensure (CC_TYPE_BACKGROUND_PREVIEW);

  panel_class->get_help_uri = cc_background_panel_get_help_uri;

  object_class->dispose = cc_background_panel_dispose;
  object_class->finalize = cc_background_panel_finalize;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/background/cc-background-panel.ui");

  gtk_widget_class_bind_template_child (widget_class, CcBackgroundPanel, background_chooser);
  gtk_widget_class_bind_template_child (widget_class, CcBackgroundPanel, default_preview);
  gtk_widget_class_bind_template_child (widget_class, CcBackgroundPanel, dark_preview);
  gtk_widget_class_bind_template_child (widget_class, CcBackgroundPanel, default_toggle);
  gtk_widget_class_bind_template_child (widget_class, CcBackgroundPanel, dark_toggle);

  gtk_widget_class_bind_template_callback (widget_class, on_color_scheme_toggle_active_cb);
  gtk_widget_class_bind_template_child (widget_class, CcBackgroundPanel, color_box);
  gtk_widget_class_bind_template_child (widget_class, CcBackgroundPanel, red);
  gtk_widget_class_bind_template_child (widget_class, CcBackgroundPanel, orange);
  gtk_widget_class_bind_template_child (widget_class, CcBackgroundPanel, yellow);
  gtk_widget_class_bind_template_child (widget_class, CcBackgroundPanel, green);
  gtk_widget_class_bind_template_child (widget_class, CcBackgroundPanel, blue);
  gtk_widget_class_bind_template_child (widget_class, CcBackgroundPanel, purple);
  gtk_widget_class_bind_template_child (widget_class, CcBackgroundPanel, pink);
  gtk_widget_class_bind_template_child (widget_class, CcBackgroundPanel, mint);

  gtk_widget_class_bind_template_callback (widget_class, on_light_dark_toggle_active_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_chooser_background_chosen_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_add_picture_button_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_red_toggled);
  gtk_widget_class_bind_template_callback (widget_class, on_orange_toggled);
  gtk_widget_class_bind_template_callback (widget_class, on_yellow_toggled);
  gtk_widget_class_bind_template_callback (widget_class, on_green_toggled);
  gtk_widget_class_bind_template_callback (widget_class, on_mint_toggled);
  gtk_widget_class_bind_template_callback (widget_class, on_blue_toggled);
  gtk_widget_class_bind_template_callback (widget_class, on_purple_toggled);
  gtk_widget_class_bind_template_callback (widget_class, on_pink_toggled);
}

static void
on_settings_changed (CcBackgroundPanel *panel)
{
  reload_current_bg (panel);
  update_preview (panel);
}

static void
cc_background_panel_init (CcBackgroundPanel *panel)
{
  g_autoptr(GSettingsSchema) schema = NULL;
  g_resources_register (cc_background_get_resource ());

  gtk_widget_init_template (GTK_WIDGET (panel));

  panel->connection = g_application_get_dbus_connection (g_application_get_default ());

  panel->thumb_factory = gnome_desktop_thumbnail_factory_new (GNOME_DESKTOP_THUMBNAIL_SIZE_LARGE);

  panel->settings = g_settings_new (WP_PATH_ID);
  g_settings_delay (panel->settings);

  panel->lock_settings = g_settings_new (WP_LOCK_PATH_ID);
  g_settings_delay (panel->lock_settings);

  schema = g_settings_schema_source_lookup (g_settings_schema_source_get_default (),
                                            SHELL_PATH_ID, 
                                            TRUE);
  if (!schema) {
    g_warning ("No user theme settings installed here. Please fix your installation.");
    return;
  }

  panel->interface_settings = g_settings_new (INTERFACE_PATH_ID);
  panel->theme_settings = g_settings_new_full (schema, NULL, NULL);

  /* Load the background */
  reload_current_bg (panel);
  update_preview (panel);

  /* Background settings */
  g_signal_connect_object (panel->settings, "changed", G_CALLBACK (on_settings_changed), panel, G_CONNECT_SWAPPED);

  /* Interface settings */
  reload_color_scheme_toggles (panel);

  g_signal_connect_object (panel->interface_settings,
                           "changed::" INTERFACE_COLOR_SCHEME_KEY,
                           G_CALLBACK (reload_color_scheme_toggles),
                           panel,
                           G_CONNECT_SWAPPED);
	
  redarr = g_variant_new ("(ddd)", redd);
  orangearr = g_variant_new ("(ddd)", oranged);
  yellowarr = g_variant_new ("(ddd)", yellowd);
  greenarr = g_variant_new ("(ddd)", greend);
  mintarr = g_variant_new ("(ddd)", mintd);
  bluearr = g_variant_new ("(ddd)", blued);
  purplearr = g_variant_new ("(ddd)", purpled);
  pinkarr = g_variant_new ("(ddd)", pinkd);

  g_signal_connect_object (panel->interface_settings,
                           "changed::" INTERFACE_ACCENT_COLOR_KEY,
                           G_CALLBACK (accent_refresh),
                           panel,
                           G_CONNECT_SWAPPED);
  accent_refresh (panel);

  g_dbus_proxy_new_for_bus (G_BUS_TYPE_SESSION,
                            G_DBUS_PROXY_FLAGS_NONE,
                            NULL,
                            "org.gnome.Shell",
                            "/org/gnome/Shell",
                            "org.gnome.Shell",
                            NULL,
                            got_transition_proxy_cb,
                            panel);

  load_custom_css (panel);
}
