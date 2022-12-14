/**
 * The rm project Copyright (c) 2012-2021 Jan-Michael Brummer
 *
 * This library is free software; you can redistribute it and/or modify it under the terms of the GNU Lesser General Public License as published by the Free Software Foundation; either version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License along with this library; if not, write to the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#pragma once

#include <glib.h>

G_BEGIN_DECLS

/** States in parser */
enum {
  STATE_NEW = 0,
  STATE_TAG,
  STATE_OPTIONS,
  STATE_ENTRY
};

struct vcard_data {
  gint state;
  char *header;
  char *options;
  char *entry;
};

struct vcard {
  GSList *data;
};

GString *vcard_create_uid (void);
void vcard_load_file (char *file_name);
void vcard_write_file (char *file_name);

G_END_DECLS
