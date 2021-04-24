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

G_BEGIN_DECLS

#define ROGER_PREFS_SCHEMA    "org.tabos.roger"
#define ROGER_SETTINGS_MAIN   roger_settings_get (ROGER_PREFS_SCHEMA)

#define ROGER_PREFS_RUN_IN_BACKGROUND       "run-in-background"

GSettings *roger_settings_get (const char *schema);

void roger_settings_shutdown (void);

G_END_DECLS
