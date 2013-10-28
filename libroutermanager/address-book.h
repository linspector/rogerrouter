/**
 * The libroutermanager project
 * Copyright (c) 2012-2013 Jan-Michael Brummer
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef LIBROUTERMANAGER_ADDRESS_BOOK_H
#define LIBROUTERMANAGER_ADDRESS_BOOK_H

#include <libroutermanager/contact.h>

G_BEGIN_DECLS

struct address_book {
	GSList *(*get_contacts)(void);
	gboolean (*remove_contact)(struct contact *contact);
	gboolean (*modify_contact)(struct contact *contact);
	gboolean (*create_contact)(struct contact *contact);
};

GSList *address_book_get_contacts(void);
gboolean address_book_remove_contact(struct contact *contact);
gboolean address_book_modify_contact(struct contact *contact);
gboolean address_book_create_contact(struct contact *contact);
void routermanager_address_book_register(struct address_book *book);

G_END_DECLS

#endif
