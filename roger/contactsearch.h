/**
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

#define CONTACT_TYPE_SEARCH (contact_search_get_type ())

G_DECLARE_FINAL_TYPE (ContactSearch, contact_search, CONTACT, SEARCH, GtkBox)

GtkWidget *contact_search_new (void);
char *contact_search_get_number (ContactSearch *widget);
void          contact_search_clear (ContactSearch *widget);
void          contact_search_set_text (ContactSearch *widget,
                                       char          *text);
const char *contact_search_get_text (ContactSearch *widget);
void         contact_search_set_contact (ContactSearch *widget,
                                         RmContact     *contact,
                                         gboolean       identify);
gchar *phone_number_type_to_string (RmPhoneNumber *number);

G_END_DECLS
