/* cc-color-button.c
 *
 * Copyright 2022 Fyra Labs
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
 * 
 * Author: Jamie Murphy <jamie@fyralabs.com
 */

#include "cc-color-button.h"

struct _CcColorButton
{
    GtkCheckButton  parent;

    gchar          *color;
};

G_DEFINE_TYPE (CcColorButton, cc_color_button, GTK_TYPE_CHECK_BUTTON)

enum
{
    PROP_0,
    PROP_COLOR,
    N_PROPS
};

static GParamSpec *props[N_PROPS] = { NULL, };

static void
cc_color_button_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
    CcColorButton *self = CC_COLOR_BUTTON (object);

    switch (prop_id)
        {
            case PROP_COLOR:
                g_value_set_string (value, cc_color_button_get_color (self));
                break;
            default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        }
}

static void
cc_color_button_set_property (GObject          *object,
                              guint             prop_id,
                              const GValue     *value,
                              GParamSpec       *pspec)
{
    CcColorButton *self = CC_COLOR_BUTTON (object);

    switch (prop_id)
        {
            case PROP_COLOR:
                cc_color_button_set_color (self, g_value_get_string(value));
                break;
            default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        }
}

static void
cc_color_button_dispose (GObject *object)
{
    CcColorButton *self = CC_COLOR_BUTTON (object);

    g_clear_pointer (&self->color, g_free);

    G_OBJECT_CLASS (cc_color_button_parent_class)->dispose (object);
}

static void
cc_color_button_class_init (CcColorButtonClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->get_property = cc_color_button_get_property;
    object_class->set_property = cc_color_button_set_property;
    object_class->dispose = cc_color_button_dispose;

    props[PROP_COLOR] = 
        g_param_spec_string ("color",
                             "Color",
                             "Color",
                             "meson-red",
                             G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

    g_object_class_install_properties (object_class, N_PROPS, props);
}

static void
cc_color_button_init (CcColorButton *self)
{
    self->color = "meson-red";
}

const gchar *
cc_color_button_get_color (CcColorButton *self)
{
  g_return_val_if_fail (CC_IS_COLOR_BUTTON (self), NULL);

  return self->color;
}

void
cc_color_button_set_color (CcColorButton *self,
                           const gchar   *color)
{
  g_return_if_fail (CC_IS_COLOR_BUTTON (self));

  gtk_widget_add_css_class (gtk_widget_get_first_child (GTK_WIDGET (self)), "fill-button");
  gtk_widget_add_css_class (gtk_widget_get_first_child (GTK_WIDGET (self)), color);

  g_object_notify_by_pspec (G_OBJECT (self), props[PROP_COLOR]);
}