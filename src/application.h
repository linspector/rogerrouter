/*
 * Roger Router
 * Copyright (c) 2012-2021 Jan-Michael Brummer
 *
 * This file is part of Roger Router.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 only.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <gtk/gtk.h>
#include <rm/rm.h>

G_BEGIN_DECLS

#define APP_GSETTINGS_SCHEMA "org.tabos.roger"

extern GSettings * app_settings;
extern GtkApplication *roger_app;

void app_show_contacts (void);
void app_show_preferences (void);
void app_show_help (void);
void app_quit (void);
void app_copy_ip (void);
void app_reconnect (void);
void app_hangup (RmConnection *connection);
void app_pickup (RmConnection *connection);
void app_present_journal (void);

G_END_DECLS