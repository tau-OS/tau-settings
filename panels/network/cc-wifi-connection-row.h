/*
 * Copyright © 2018 Red Hat Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <gtk/gtk.h>
#include <NetworkManager.h>

G_BEGIN_DECLS

typedef struct _CcWifiConnectionRow CcWifiConnectionRow;

#define CC_TYPE_WIFI_CONNECTION_ROW (cc_wifi_connection_row_get_type ())
G_DECLARE_FINAL_TYPE (CcWifiConnectionRow, cc_wifi_connection_row, CC, WIFI_CONNECTION_ROW, GtkListBoxRow)

CcWifiConnectionRow *cc_wifi_connection_row_new              (NMDeviceWifi  *device,
                                                              NMConnection  *connection,
                                                              GPtrArray     *aps,
                                                              gboolean       checkable);

gboolean             cc_wifi_connection_row_get_checkable    (CcWifiConnectionRow  *row);
gboolean             cc_wifi_connection_row_get_checked      (CcWifiConnectionRow  *row);
NMDeviceWifi        *cc_wifi_connection_row_get_device       (CcWifiConnectionRow  *row);
NMAccessPoint       *cc_wifi_connection_row_get_access_point (CcWifiConnectionRow  *row);
NMConnection        *cc_wifi_connection_row_get_connection   (CcWifiConnectionRow  *row);

G_END_DECLS