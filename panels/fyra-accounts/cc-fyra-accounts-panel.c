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

#include "cc-fyra-accounts-panel.h"
#include "cc-fyra-accounts-resources.h"
#include "shell/cc-application.h"
#include "shell/cc-object-storage.h"

struct _CcFyraAccountsPanel {
  CcPanel                 parent_instance;
};

CC_PANEL_REGISTER (CcFyraAccountsPanel, cc_fyra_accounts_panel);

static void
cc_fyra_accounts_panel_dispose (GObject *object)
{
//   CcFyraAccountsPanel *self = CC_FYRA_ACCOUNTS_PANEL (object);

  G_OBJECT_CLASS (cc_fyra_accounts_panel_parent_class)->dispose (object);
}

static void
cc_fyra_accounts_panel_class_init (CcFyraAccountsPanelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = cc_fyra_accounts_panel_dispose;

  gtk_widget_class_set_template_from_resource (widget_class, 
                                               "/org/gnome/control-center/fyra-accounts/cc-fyra-accounts-panel.ui");
}

static void
cc_fyra_accounts_panel_init (CcFyraAccountsPanel *self)
{
  g_resources_register (cc_fyra_accounts_get_resource ());
  gtk_widget_init_template (GTK_WIDGET (self));
}
