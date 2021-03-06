/* cc-file-chooser-button.c
 *
 * Copyright 2021 Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "ws-file-chooser-button.h"

#include <glib/gi18n.h>

struct _WsFileChooserButton
{
  GtkButton parent_instance;

  GtkFileChooser *filechooser;
  GFile *file;
  char *title;
};

G_DEFINE_FINAL_TYPE (WsFileChooserButton, ws_file_chooser_button, GTK_TYPE_BUTTON)

enum
{
  PROP_0,
  PROP_FILE,
  PROP_TITLE,
  N_PROPS
};

static GParamSpec *properties [N_PROPS] = { NULL, };

static const char *
get_title (WsFileChooserButton *self)
{
  return self->title ? self->title : _("Select a file");
}

static void
update_label (WsFileChooserButton *self)
{
  g_autofree gchar *label = NULL;

  if (self->file)
    label = g_file_get_basename (self->file);
  else
    label = g_strdup (get_title (self));

  gtk_button_set_label (GTK_BUTTON (self), label);
}

static void
on_filechooser_dialog_response_cb (GtkFileChooser      *filechooser,
                                   gint                 response,
                                   WsFileChooserButton *self)
{
  if (response == GTK_RESPONSE_ACCEPT)
    {
      g_autoptr(GFile) file = NULL;

      file = gtk_file_chooser_get_file (filechooser);
      ws_file_chooser_button_set_file (self, file);
    }

  gtk_widget_hide (GTK_WIDGET (filechooser));
}
static void
ensure_filechooser (WsFileChooserButton *self)
{
  GtkNative *native;
  GtkWidget *dialog;

  if (self->filechooser)
    return;

  native = gtk_widget_get_native (GTK_WIDGET (self));

  dialog = gtk_file_chooser_dialog_new (get_title (self),
                                        GTK_WINDOW (native),
                                        GTK_FILE_CHOOSER_ACTION_OPEN,
                                        _("Cancel"),
                                        GTK_RESPONSE_CANCEL,
                                        _("Open"),
                                        GTK_RESPONSE_ACCEPT,
                                        NULL);
  gtk_window_set_destroy_with_parent (GTK_WINDOW (dialog), TRUE);
  gtk_window_set_hide_on_close (GTK_WINDOW (dialog), TRUE);
  gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);

  if (self->file)
    gtk_file_chooser_set_file (GTK_FILE_CHOOSER (dialog), self->file, NULL);

  g_signal_connect (dialog, "response", G_CALLBACK (on_filechooser_dialog_response_cb), self);

  self->filechooser = GTK_FILE_CHOOSER (dialog);
}


static void
ws_file_chooser_button_clicked (GtkButton *button)
{
  WsFileChooserButton *self = WS_FILE_CHOOSER_BUTTON (button);
  GtkNative *native = gtk_widget_get_native (GTK_WIDGET (self));

  ensure_filechooser (self);

  gtk_window_set_transient_for (GTK_WINDOW (self->filechooser), GTK_WINDOW (native));
  gtk_window_present (GTK_WINDOW (self->filechooser));
}

static void
ws_file_chooser_button_finalize (GObject *object)
{
  WsFileChooserButton *self = (WsFileChooserButton *)object;

  g_clear_pointer (&self->title, g_free);
  g_clear_object (&self->file);

  G_OBJECT_CLASS (ws_file_chooser_button_parent_class)->finalize (object);
}

static void
ws_file_chooser_button_get_property (GObject    *object,
                                     guint       prop_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
  WsFileChooserButton *self = WS_FILE_CHOOSER_BUTTON (object);

  switch (prop_id)
    {
    case PROP_FILE:
      g_value_set_object (value, self->file);
      break;

    case PROP_TITLE:
      g_value_set_string (value, self->title);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ws_file_chooser_button_set_property (GObject      *object,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  WsFileChooserButton *self = WS_FILE_CHOOSER_BUTTON (object);

  switch (prop_id)
    {
    case PROP_FILE:
      ws_file_chooser_button_set_file (self, g_value_get_object (value));
      break;

    case PROP_TITLE:
      ws_file_chooser_button_set_title (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ws_file_chooser_button_class_init (WsFileChooserButtonClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkButtonClass *button_class = GTK_BUTTON_CLASS (klass);

  button_class->clicked = ws_file_chooser_button_clicked;

  object_class->finalize = ws_file_chooser_button_finalize;
  object_class->get_property = ws_file_chooser_button_get_property;
  object_class->set_property = ws_file_chooser_button_set_property;

  properties[PROP_FILE] = g_param_spec_object ("file", "", "",
                                               G_TYPE_FILE,
                                               G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  properties[PROP_TITLE] = g_param_spec_string ("title", "", "", NULL,
                                                G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ws_file_chooser_button_init (WsFileChooserButton *self)
{
  update_label (self);
}

GtkWidget *
ws_file_chooser_button_new (void)
{
  return g_object_new (WS_TYPE_FILE_CHOOSER_BUTTON, NULL);
}

void
ws_file_chooser_button_set_file (WsFileChooserButton *self,
                                 GFile               *file)
{
  g_return_if_fail (WS_IS_FILE_CHOOSER_BUTTON (self));

  if (g_set_object (&self->file, file))
    {
      gtk_file_chooser_set_file (self->filechooser, file, NULL);
      update_label (self);

      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_FILE]);
    }
}

GFile *
ws_file_chooser_button_get_file (WsFileChooserButton *self)
{
  g_return_val_if_fail (WS_IS_FILE_CHOOSER_BUTTON (self), NULL);

  return self->file ? g_object_ref (self->file) : NULL;
}

void
ws_file_chooser_button_set_title (WsFileChooserButton *self,
                                  const char          *title)
{
  g_autofree char *old_title = NULL;

  g_return_if_fail (WS_IS_FILE_CHOOSER_BUTTON (self));

  old_title = g_steal_pointer (&self->title);
  self->title = g_strdup (title);

  update_label (self);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_TITLE]);
}

const char *
ws_file_chooser_button_get_title (WsFileChooserButton *self)
{
  g_return_val_if_fail (WS_IS_FILE_CHOOSER_BUTTON (self), NULL);

  return self->title;
}

GtkFileChooser *
ws_file_chooser_button_get_filechooser (WsFileChooserButton *self)
{
  g_return_val_if_fail (WS_IS_FILE_CHOOSER_BUTTON (self), NULL);

  ensure_filechooser (self);

  return self->filechooser;
}
