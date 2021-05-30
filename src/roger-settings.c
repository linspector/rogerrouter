/*
 * Roger Router Copyright (c) 2012-2021 Jan-Michael Brummer
 *
 * This file is part of Roger Router.
 *
 * This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation; version 2 only.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "roger-settings.h"

#include <glib.h>

static GHashTable *settings = NULL;

static void
roger_settings_init (void)
{
  if (settings)
    return;

  settings = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);
}

GSettings *
roger_settings_get (const char *schema)
{
  GSettings *gsettings = NULL;

  roger_settings_init ();

  gsettings = g_hash_table_lookup (settings, schema);
  if (gsettings)
    return gsettings;

  gsettings = g_settings_new (schema);
  if (!gsettings)
    g_warning ("Invalid schema %s requested", schema);
  else
    g_hash_table_insert (settings, g_strdup (schema), gsettings);

  return gsettings;
}

void
roger_settings_shutdown (void)
{
  if (!settings)
    return;

  g_hash_table_remove_all (settings);
  g_hash_table_unref (settings);
}
