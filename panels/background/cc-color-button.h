/* cc-color-button.h
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

#pragma once

#include <glib.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define CC_TYPE_COLOR_BUTTON (cc_color_button_get_type ())
G_DECLARE_FINAL_TYPE (CcColorButton, cc_color_button, CC, COLOR_BUTTON, GtkFlowBoxChild)

const gchar *cc_color_button_get_color (CcColorButton *self);
void         cc_color_button_set_color (CcColorButton *self,
                                        const gchar   *color);

void cc_color_button_activate (CcColorButton *self);

G_END_DECLS