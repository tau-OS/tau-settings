/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 *
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

#pragma once

#include <shell/cc-panel.h>

G_BEGIN_DECLS

#define CC_TYPE_SYSTEM_PANEL (cc_system_panel_get_type ())
G_DECLARE_FINAL_TYPE (CcSystemPanel, cc_system_panel, CC, SYSTEM_PANEL, CcPanel)

GtkWidget *cc_system_panel_new (void);

G_END_DECLS
